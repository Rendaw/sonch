#include "core.h"

#define SplitDir "splits"
#define HostInstanceIndex 0
#define NullIndex

DefineProtocol(StaticDataProtocol);
DefineProtocolVersion(StaticDataV1, StaticDataProtocol);
DefineProtocolMesssage(StaticDataV1All, StaticDataV1, void(std::string InstanceName, UUID InstanceUUID));

CoreDatabase::CoreDatabase(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID)
{
	if (Create)
	{
		Execute("CREATE TABLE \"Stats\" "
		"("
			"\"Version\" INTEGER"
		")");
		Execute("INSERT INTO \"Stats\" VALUES (?)", DatabaseVersion::Latest);
	}

	if (Create)
	{
		Execute("CREATE TABLE \"Instances\" "
		"("
			"\"Index\" INTEGER PRIMARY KEY AUTOINCREMENT, "
			"\"ID\" INTEGER, "
			"\"Name\" VARCHAR"
		")");
		Execute("CREATE INDEX \"IDIndex\" ON \"Instances\" "
		"("
			"\"ID\" ASC"
		")");
		Execute("INSERT INTO \"Instances\" (\"ID\", \"Name\") VALUES (?, ?)", InstanceName, InstanceID);
	}

	Begin = Prepare<void(void)>("BEGIN");
	End = Prepare<void(void)>("COMMIT");

	if (Create)
	{
		Execute("CREATE TABLE \"Counters\" "
		"("
			"\"File\" INTEGER , "
			"\"Change\" INTEGER , "
		")");
		Execute("INSERT INTO \"Counters\" VALUES (?, ?)", NullIndex + 1, NullIndex + 1);
	}
	GetFileCounter = Prepare<UUID(void)>("SELECT \"File\" FROM \"Counters\"");
	IncrementFileCounter = Prepare<void(void)>("UPDATE \"Counters\" SET \"File\" = \"File\" + 1");
	GetChangeCounter = Prepare<UUID(void)>("SELECT \"Change\" FROM \"Counters\"");
	IncrementChangeCounter = Prepare<void(void)>("UPDATE \"Counters\" SET \"Change\" = \"Change\" + 1");

	if (Create)
	{
		Execute("CREATE TABLE \"Files\" "
		"("
			"\"IDInstance\" INTEGER , "
			"\"IDIndex\" INTEGER , "
			"\"ChangeInstance\" INTEGER , "
			"\"ChangeIndex\" INTEGER , "
			"\"ParentInstance\" INTEGER , "
			"\"ParentIndex\" INTEGER , "
			"\"Name\" VARCHAR , "
			"\"IsFile\" BOOLEAN , "
			"\"Modified\" DATETIME , "
			"\"Permissions\" BLOB , "
			"\"IsSplit\" BOOLEAN , "
			"PRIMARY KEY (\"Instance\", \"ID\")"
		")");
		Execute("CREATE INDEX \"ParentIndex\" ON \"Files\" "
		"("
			"\"ParentInstance\" ASC, "
			"\"ParentID\" ASC, "
			"\"Name\" ASC"
		")");
	}
	GetFileByID = Prepare<ShareFileTuple(Counter IDInstance, UUID IDIndex)>>
		("SELECT FROM \"Files\" WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"IsSplit\" = 0 LIMIT 1");
	GetFile = Prepare<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, std::string Name)>>
		("SELECT FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 AND \"Name\" = ? LIMIT 1");
	GetSplitFile = Prepare<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter SplitInstance, std::string Name)>>
		("SELECT FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 1 AND \"ChangeInstance\" = ? AND \"Name\" = ? LIMIT 1");
	GetFiles = Prepare<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter Offset, Counter Limit)>>
		("SELECT FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 OFFSET ? LIMIT ?");
	GetSplitFiles = Prepare<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter SplitInstance, Counter Offset, Counter Limit)>>
		("SELECT FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 AND \"ChangeInstance\" = ? OFFSET ? LIMIT ?");
	CreateFile = Prepare<void(Counter, UUID, Counter, UUID, std::string, bool, Timestamp, SharePermissions)>
		("INSERT OR IGNORE INTO \"Files\" VALUES (?, ?, 0, 0, ?, ?, ?, ?, ?, ?, 0);");
	DeleteFile = Prepare<void(Counter, UUID, Counter, UUID)>
		("DELETE FROM \"Files\" WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?");
	SetPermissions = Prepare<void(SharePermissionsin Counter, UUID, Counter, UUID, Counter, UUID)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"Permissions\" = ? WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?");
	SetTimestamp = Prepare<void(Counter, UUID, Timestamp, Counter, UUID, Counter, UUID)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"Modified\" = ? WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?");
	MoveFile = Prepare<void(Counter, UUID, Counter, UUID, std::string, Counter, UUID, Counter, UUID)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"ParentInstance\" = ?, \"ParentIndex\" = ?, \"Name\" = ?, WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?");
	if (Create)
		CreateFile(HostInstanceIndex, NullIndex, HostInstanceIndex, NullIndex, HostInstanceIndex, NullIndex, "", std::time(), false, SharePermissions{1, 1, 1, 1, 0, 1, 1, 0, 1});

	if (Create)
	{
		Execute("CREATE TABLE \"Ancestry\" "
		"("
			"\"IDInstance\" INTEGER , "
			"\"IDIndex\" INTEGER , "
			"\"ParentInstance\" INTEGER , "
			"\"ParentIndex\" INTEGER , "
			"PRIMARY KEY (\"Instance\", \"ID\")"
		")");
	}
	CreateChange = Prepare<void(Counter, UUID, Counter, UUID)>
		("INSERT OR IGNORE INTO \"Ancestry\" VALUES (?, ?, ?, ?)");
}

CoreTransactor::CoreTransactor(std::mutex &CoreMutex) :
	Transactor(CoreMutex, Create, SetPermissions, SetTimestamp, Delete, Move),
	Create([this](
		UUID const &FileIndex,
		ShareFile const &Parent, std::string const &Name, bool const &IsFile,
		bool const &OwnerRead, bool const &OwnerWrite, bool const &OwnerExecute,
		bool const &GroupRead, bool const &GroupWrite, bool const &GroupExecute,
		bool const &OtherRead, bool const &OtherWrite, bool const &OtherExecute)
	{
		if (Get(Path)) return;
		uint64_t Time = std::time();
		FilePermissions Permissions{
			OwnerRead, OwnerWrite, OwnerExecute,
			GroupRead, GroupWrite, GroupExecute,
			OtherRead, OtherWrite, OtherExecute};
		Statement.CreateFile(HostInstanceIndex, FileIndex, Parent.Instance, Parent.ID, Name, Time, IsFile, Permissions);
		if (IsFile)
		{
			auto const InternalFilename = FilePath / GetInternalFilename(
				NodeID{HostInstanceIndex, FileIndex},
				NodeID{HostInstanceIndex, NullIndex});
			bfs::ofstream Out(InternalFilename, std::ofstream::out | std::ofstream::binary);
			if (!Out) throw ActionError{ActionError::Unknown};
		}
	}),
	SetPermissions(),
	SetTimestamp(),
	Delete(),
	Move()
{
}

static std::string GetInternalFilename(NodeID const &ID, NodeID const &Change)
	{ return String() << ID.Instance << "-" ID.Index << "-" << Change.Instance << "-" << Change.Index; }

ShareCore::ShareCore(bfs::path const &Root, std::string const &InstanceName = std::string()) :
	Root(Root), InstanceName(InstanceName)
{
	auto const ValidateFilename = [](std::string const &Filename)
	{
		if (Filename.empty()) return false;
		for (char const FilenameChar : Filename)
		{
			switch (FilenameChar)
			{
				case '\0':
				case '/':
#ifndef CUSTOM_STRANGE_PATHS
				case '\\': case ':': case '*': case '?': case '"': case '<': case '>': case '|':
#endif
					return false;
				default: break;
			}
		}
		return true;
	};
	auto const GetInstanceFilename = [](std::string const &Name, UUID ID)
	{
		std::vector<char> Buffer;
		Buffer.resize(Name.size() + sizeof(ID) * 2 + 1);
		memcpy(&Buffer[0], Name.c_str(), Name.size());
		Buffer[Name.size()] = '-';
		for (unsigned int Offset = 0; Offset < sizeof(ID); ++Offset)
		{
			Buffer[Name.size() + 1 + Offset * 2] = 'a' + (*(reinterpret_cast<char *>(&ID) + Offset) & 0xF);
			Buffer[Name.size() + 1 + Offset * 2 + 1] = 'a' + ((*(reinterpret_cast<char *>(&ID) + Offset) & 0xF0) >> 4);
		}
		return std::string(Buffer.begin(), Buffer.end());
	};

	try
	{
		bfs::path const DatabasePath = RootPath / "." App / "database";
		enum DatabaseVersion()
		{
			V1 = 0,
			End,
			Latest = End - 1
		};

		bfs::path const TransactionPath = RootPath / "." App / "transactions";

		FilePath = RootPath / "." App / "files";

		if (!bfs::exists(RootPath))
		{
			// Try to create an empty share, since the target didn't exist
			// --
			if (InstanceName.empty())
				{ throw UserError() << "Share '" << RootPath.string() << "' does not exist.  Specify NAME to create a new share."; }
			if (!ValidateFilename(InstanceName))
				{ throw UserError() << "Instance NAME contains invalid characters."; }

			auto RandomGenerator = std::mt19937_64{std::random_device{}()};
			InstanceID = std::uniform_int_distribution<UUID>()(RandomGenerator);

			InstanceFilename = GetInstanceFilename(InstanceName, InstanceID);

			// Prepare the base file hierarchy
			bfs::create_directory(RootPath);
			bfs::create_directory(RootPath / "." App);
			bfs::create_directory(FilePath);
			bfs::create_directory(TransactionPath);

			{
				bfs::path StaticDataPath = RootPath / "." App / "static";
				bfs::ofstream Out(StaticDataPath, std::ofstream::out | std::ofstream::binary);
				if (!Out) throw SystemError() << "Could not create file '" << StaticDataPath << "'.";
				auto const &Data = StaticDataV1All::Write(InstanceName, InstanceID);
				Out.write((char const *)&Data[0], Data.size());
			}

			{
				bfs::path ReadmePath = RootPath / App "-share-readme.txt";
				bfs::ofstream Out(ReadmePath);
				if (!Out) throw SystemError() << "Could not create file '" << ReadmePath << "'.";
				Out << "Do not modify the contents of this directory.\n\nThis directory is the unmounted data for a " App " share.  Modifying the contents could cause data corruption.  It is safe to move and change the ownership for this folder (but not it's permissions or contents)." << std::endl;
			}

			Database.reset(new CoreDatabase(DatabasePath, true));
		}
		else
		{
			// Share exists, restore state
			// --
			if (!InstanceName.empty())
				Log.Warn() << "Share exists, ignoring all other arguments.";

			// Load static data
			bfs::ifstream In(RootPath / "." App / "static", std::ifstream::in | std::ifstream::binary);
			Protocol::Reader<StandardOutLog, StaticDataV1All> Reader(Log,
			[&](std::string &ReadInstanceName, UUID &ReadInstanceUUID)
			{
				InstanceName = ReadInstanceName;
				InstanceID = ReadInstanceUUID;
			});
			bool Success = Reader.Read(In);
			if (!Success)
				throw SystemError() << "Could not read static data, file may be corrupt.";

			InstanceFilename = GetInstanceFilename(InstanceName, InstanceID);

			Database.reset(new CoreDatabase(DatabasePath, false));
			unsigned int Version = Database->Get<unsigned int()>("SELECT \"Version\" FROM \"Stats\"");
			switch (Version)
			{
				default: throw SystemError() << "Unrecognized database version " << Version;
				/*case Last - 2...: Do upgrade to Last - 1;
				case Last - 2...: Do upgrade to Last;*/
				case Last: break;
			}
			Database->Execute("UPDATE \"Stats\" SET \"Version\" = ?", DatabaseVersion::Latest);
		}

		Transaction.Create = [this](
			UUID const &FileIndex,
			ShareFile const &Parent, std::string const &Name, bool const &IsFile,
			bool const &OwnerRead, bool const &OwnerWrite, bool const &OwnerExecute,
			bool const &GroupRead, bool const &GroupWrite, bool const &GroupExecute,
			bool const &OtherRead, bool const &OtherWrite, bool const &OtherExecute)
		{
			if (Get(Path)) return;
			uint64_t Time = std::time();
			FilePermissions Permissions{
				OwnerRead, OwnerWrite, OwnerExecute,
				GroupRead, GroupWrite, GroupExecute,
				OtherRead, OtherWrite, OtherExecute};
			Statement.CreateFile(HostInstanceIndex, FileIndex, Parent.Instance, Parent.ID, Name, Time, IsFile, Permissions);
			if (IsFile)
			{
				auto const InternalFilename = FilePath / GetInternalFilename(
					NodeID{HostInstanceIndex, FileIndex},
					NodeID{HostInstanceIndex, NullIndex});
				bfs::ofstream Out(InternalFilename, std::ofstream::out | std::ofstream::binary);
				if (!Out) throw ActionError{ActionError::Unknown};
			}
		};

		Transaction.SetPermissions = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			bool const &OwnerRead, bool const &OwnerWrite, bool const &OwnerExecute,
			bool const &GroupRead, bool const &GroupWrite, bool const &GroupExecute,
			bool const &OtherRead, bool const &OtherWrite, bool const &OtherExecute)
		{
			Statement.SetPermissions(
				HostInstanceIndex, NewChangeIndex,
				Permissions{
					OwnerRead, OwnerWrite, OwnerExecute,
					GroupRead, GroupWrite, GroupExecute,
					OtherRead, OtherWrite, OtherExecute},
				File.ID().Instance, File.ID().Index,
				File.Change().Instance, File.Change().Index);
			Statement.CreateChange(
				File.Change().Instance, File.Change().Index,
				HostInstanceIndex, NewChangeIndex);
			if (File.IsFile)
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), NodeID{HostInstanceIndex, NewChangeIndex}));
		};

		Transaction.SetTimestamp = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			uint64_t const &Timestamp)
		{
			Statement.SetTimestamp(
				HostInstanceIndex, NewChangeIndex,
				Timestamp,
				File.ID().Instance, File.ID().Index,
				File.Change().Instance, File.Change().Index);
			Statement.CreateChange(
				File.Change().Instance, File.Change().Index,
				HostInstanceIndex, NewChangeIndex);
			if (File.IsFile)
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), NodeID{HostInstanceIndex, NewChangeIndex}));
		};

		Transaction.Delete = [this](ShareFile const &File)
		{
			Statement.DeleteFile(File.Instance, File.ID, File.ChangeInstance, File.ChangeIndex);
			if (File.IsFile)
				bfs::remove(FilePath / GetInternalFilename(File.ID(), File.Change()));
		};

		Transaction.Move = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			NodeID const &Parent,
			std::string const &Name)
		{
			Statement.MoveFile(
				HostInstanceIndex, NewChangeIndex, Parent.Instance, Parent.Index, Name,
				File.ID().Instance, File.ID().Index,
				File.Change().Instance, File.Change().Index);
			Statement.CreateChange(
				File.Change().Instance, File.Change().Index,
				HostInstanceIndex, NewChangeIndex);
			if (File.IsFile)
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), NodeID{HostInstanceIndex, NewChangeIndex}));
		};

		Transact.reset(new Transactor(Mutex, TransactionPath,
			Transaction.CreateFile,
			Transaction.SetPermissions,
			Transaction.SetTimestamp,
			Transaction.Delete,
			Transaction.Move));
	}
	catch (bfs::filesystem_error &Error)
		{ throw SystemError() << Error.what(); }
}

bfs::path ShareCore::GetRoot(void) const { return Root; }

unsigned int ShareCore::GetUser(void) const { return geteuid(); }

unsigned int ShareCore::GetGroup(void) const { return geteuid(); }

std::unique_ptr<ShareFile> ShareCore::Get(NodeID const &ID)
{
	// Get primary instance of file by file id
	Optional<ShareFile> Got = Database.GetFileByID(ID.Instance, ID.Index);
	if (!Got) return nullptr;
	return new ShareFile(Got);
}

ActionResult<std::unique_ptr<ShareFile>> ShareCore::Get(bfs::path const &Path)
{
	Optional<ShareFile> ParentFile = Database.GetFile(NodeID{0, 0}, "");
	assert(ParentFile);
	if (Path.empty()) return new ShareFile(ParentFile);

	unsigned int PathIndex = 1;
	Counter SplitInstance = 0;
	if ((Path.size() > 0) && (Path[0] == SplitPath))
	{
		if (Path.size() == 1) return new ShareFile(SplitFile);
		auto SplitInstance = InstanceMap.find(Path[1]);
		if (Instance != InstanceMap.end())
			PathInstanceIndex = Instance->second;
		if (!HasSplits(SplitInstance, ParentFile)) return ActionError::Missing;
		if (Path.size() == 2) return new ShareFile(SplitInstanceFile(PathInstanceIndex));
		PathIndex = 3;
	}

	for (; PathIndex + 1 < Path.size(); ++PathIndex)
	{
		ParentFile = Database.GetFile(ParentFile, Path[PathIndex]);
		if (!ParentFile)
			return ActionError::Missing;
		if (ParentFile.IsFile)
			return ActionError::Invalid;
		if (!HasSplits(SplitInstance, ParentFile))
			return ActionError::Missing;
	}

	auto Out = (SplitInstance == 0) ?
		Database.GetFile(ParentFile, Path[PathIndex]) :
		Database.GetSplitFile(ParentFile, SplitIndex, Path[PathIndex]);
	if (!Out) return ActionError::Missing;
	return new ShareFile(Out);
}

std::vector<std::unique_ptr<ShareFile>> ShareCore::GetChildren(ShareFile const &File, unsigned int From, unsigned int Count)
{
	std::vector<std::unique_ptr<ShareFile>> Out;
	if (File.IsSplit) Database.GetSplitFilesByParent.Execute(File.ID, File.Change.Instance, From, Count,
		[&Out](
			Counter &IDInstance, UUID &IDIndex,
			Counter &ChangeInstance, UUID &ChangeIndex,
			Counter &ParentInstance, UUID &ParentIndex,
			std::string &Name, bool &IsFile, Timestamp &Modified,
			SharePermissions &Permissions, bool IsSplit)
		{
			Out.push_back(new ShareFile{
				NodeID{IDInstance, IDIndex},
				NodeID{ChangeInstance, ChangeIndex},
				NodeID{ParentInstance, ParentIndex},
				Name, IsFile, Modified, Permissions, IsSplit});
		});
	else Database.GetFilesByParent.Execute(File.ID, From, Count,
		[&Out](
			Counter &IDInstance, UUID &IDIndex,
			Counter &ChangeInstance, UUID &ChangeIndex,
			Counter &ParentInstance, UUID &ParentIndex,
			std::string &Name, bool &IsFile, Timestamp &Modified,
			SharePermissions &Permissions, bool IsSplit)
		{
			Out.push_back(new ShareFile{
				NodeID{IDInstance, IDIndex},
				NodeID{ChangeInstance, ChangeIndex},
				NodeID{ParentInstance, ParentIndex},
				Name, IsFile, Modified, Permissions, IsSplit});
		});
	assert(Out.size() <= Count);
	return Out;
}

std::unique_ptr<ShareFile> ShareCore::Create(bfs::path const &Path, bool IsFile,
	bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
	bool GroupRead, bool GroupWrite, bool GroupExecute,
	bool OtherRead, bool OtherWrite, bool OtherExecute)
{
	Database.Begin();
	uint64_t FileIndex = Database.GetFileID();
	Database.IncrementFileID();
	Database.End();
	auto Parent = Get(Path.parent());
	if (!Parent) throw ActionError{ActionError::Missing};
	if (Parent->IsFile) throw ActionError{ActionError::Invalid};
	Transact(Transaction.Create,
		FileIndex, Parent, Path.file(), IsFile,
		OwnerRead, OwnerWrite, OwnerExecute,
		GroupRead, GroupWrite, GroupExecute,
		OtherRead, OtherWrite, OtherExecute);
}

void ShareCore::SetPermissions(ShareFile const &File,
	bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
	bool GroupRead, bool GroupWrite, bool GroupExecute,
	bool OtherRead, bool OtherWrite, bool OtherExecute)
{
	Database.Begin();
	uint64_t ChangeIndex = Database.GetChangeIndex();
	Database.IncrementChangeIndex();
	Database.End();
	Transact(Transaction.SetPermissions,
		File.Instance, File.ID, ChangeIndex,
		OwnerRead, OwnerWrite, OwnerExecute,
		GroupRead, GroupWrite, GroupExecute,
		OtherRead, OtherWrite, OtherExecute);
}

void ShareCore::SetTimestamp(ShareFile const &File, unsigned int Timestamp)
{
	Database.Begin();
	uint64_t ChangeIndex = Database.GetChangeIndex();
	Database.IncrementChangeIndex();
	Database.End();
	Transact(Transaction.SetTimestamp,
		File.Instance, File.ID, ChangeIndex,
		Timestamp);
}

void ShareCore::Delete(ShareFile const &File)
{
	Transact(Transaction.Delete, File);
}

void ShareCore::Move(ShareFile const &File, bfs::path const &To)
{
	Database.Begin();
	UUID ChangeIndex = Database.GetChangeIndex();
	Database.IncrementChangeIndex();
	Database.End();

	auto ToFile = Get(To);
	if (!ToFile)
	{
		ToFile = Get(To.parent());
		if (!ToFile) throw ActionError{ActionError::Missing};
		if (ToFile->IsFile) throw ActionError{ActionError::Invalid};
	}

	if (ToFile->IsFile)
	{
		Transact(Transaction.Move, File, ToFile->ParentInstance, ToFile->ParentID, ToFile.Name);
		Delete(ToFile);
	}
	else Transact(Transaction.Move, File, ToFile->Instance, ToFile->ID, File.Name);
}

