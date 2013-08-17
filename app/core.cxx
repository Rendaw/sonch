#include "core.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#define SplitDir "splits"
#define HostInstanceIndex 0
#define NullIndex

DefineProtocol(StaticDataProtocol);
DefineProtocolVersion(StaticDataV1, StaticDataProtocol);
DefineProtocolMessage(StaticDataV1All, StaticDataV1, void(std::string InstanceName, UUID InstanceUUID));

enum class DatabaseVersion : unsigned int
{
	V1 = 0,
	End,
	Latest = End - 1
};

CoreDatabaseStructure::CoreDatabaseStructure(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID)
{
	if (Create)
	{
		Execute("CREATE TABLE \"Stats\" "
		"("
			"\"Version\" INTEGER"
		")");
		Execute("INSERT INTO \"Stats\" VALUES (?)", (unsigned int)DatabaseVersion::Latest);

		Execute("CREATE TABLE \"Instances\" "
		"("
			"\"Index\" INTEGER PRIMARY KEY AUTOINCREMENT, "
			"\"ID\" INTEGER, "
			"\"Name\" VARCHAR, "
			"\"Filename\" VARCHAR"
		")");
		/*Execute("CREATE INDEX \"IDIndex\" ON \"Instances\" "
		"("
			"\"ID\" ASC"
		")");*/
		Execute("CREATE INDEX \"FilenameIndex\" ON \"Instances\" "
		"("
			"\"Filename\" ASC"
		")");
		Execute("INSERT INTO \"Instances\" (\"ID\", \"Name\", \"Filename\") VALUES (?, ?, ?)", InstanceName, InstanceName, InstanceID);

		Execute("CREATE TABLE \"Counters\" "
		"("
			"\"File\" INTEGER , "
			"\"Change\" INTEGER"
		")");
		Execute("INSERT INTO \"Counters\" VALUES (?, ?)", NullIndex + 1, NullIndex + 1);

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
			"PRIMARY KEY (\"IDInstance\", \"IDIndex\")"
		")");
		Execute("CREATE INDEX \"ParentIndex\" ON \"Files\" "
		"("
			"\"ParentInstance\" ASC, "
			"\"ParentIndex\" ASC, "
			"\"Name\" ASC"
		")");

		Execute("CREATE TABLE \"Ancestry\" "
		"("
			"\"IDInstance\" INTEGER , "
			"\"IDIndex\" INTEGER , "
			"\"ParentInstance\" INTEGER , "
			"\"ParentIndex\" INTEGER , "
			"PRIMARY KEY (\"IDInstance\", \"IDIndex\")"
		")");
	}
}

CoreDatabase::CoreDatabase(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID) :
	CoreDatabaseStructure(DatabasePath, Create, InstanceName, InstanceID),
	Begin(Prepare<void(void)>("BEGIN")),
	End(Prepare<void(void)>("COMMIT")),
	GetFileIndex(Prepare<UUID(void)>("SELECT \"File\" FROM \"Counters\"")),
	IncrementFileIndex(Prepare<void(void)>("UPDATE \"Counters\" SET \"File\" = \"File\" + 1")),
	GetChangeIndex(Prepare<UUID(void)>("SELECT \"Change\" FROM \"Counters\"")),
	IncrementChangeIndex(Prepare<void(void)>("UPDATE \"Counters\" SET \"Change\" = \"Change\" + 1")),
	GetInstanceIndex(Prepare<Counter(std::string Filename)>("SELECT \"Index\" FROM \"Instances\" WHERE \"Filename\" = ?")),
	GetFileByID(Prepare<ShareFileTuple(NodeID ID)>
		("SELECT * FROM \"Files\" WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"IsSplit\" = 0 LIMIT 1")),
	GetFile(Prepare<ShareFileTuple(NodeID Parent, std::string Name)>
		("SELECT * FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 AND \"Name\" = ? LIMIT 1")),
	GetSplitFile(Prepare<ShareFileTuple(NodeID Parent, Counter SplitInstance, std::string Name)>
		("SELECT * FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 1 AND \"ChangeInstance\" = ? AND \"Name\" = ? LIMIT 1")),
	GetFiles(Prepare<ShareFileTuple(NodeID Parent, Counter Offset, Counter Limit)>
		("SELECT * FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 LIMIT ?, ?")),
	GetSplitFiles(Prepare<ShareFileTuple(NodeID Parent, Counter SplitInstance, Counter Offset, Counter Limit)>
		("SELECT * FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 AND \"ChangeInstance\" = ? LIMIT ?, ?")),
	CreateFile(Prepare<void(NodeID ID, NodeID Parent, std::string Name, bool IsFile, Timestamp ModifiedTime, SharePermissions Permissions)>
		("INSERT OR IGNORE INTO \"Files\" VALUES (?, ?, 0, 0, ?, ?, ?, ?, ?, ?, 0)")),
	DeleteFile(Prepare<void(NodeID ID, NodeID Change)>
		("DELETE FROM \"Files\" WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?")),
	SetPermissions(Prepare<void(NodeID NewChange, SharePermissions NewPermissions, NodeID ID, NodeID Change)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"Permissions\" = ? WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?")),
	SetTimestamp(Prepare<void(NodeID NewChange, Timestamp NewModifiedTime, NodeID ID, NodeID Change)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"Modified\" = ? WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?")),
	MoveFile(Prepare<void(NodeID NewChange, NodeID NewParent, std::string NewName, NodeID ID, NodeID Change)>
		("UPDATE \"Files\" SET \"ChangeInstance\" = ?, \"ChangeIndex\" = ?, \"ParentInstance\" = ?, \"ParentIndex\" = ?, \"Name\" = ? WHERE \"IDInstance\" = ? AND \"IDIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?")),
	CreateChange(Prepare<void(NodeID NewChange, NodeID OldChange)>
		("INSERT OR IGNORE INTO \"Ancestry\" VALUES (?, ?, ?, ?)"))
{
	if (Create)
		CreateFile(NodeID(), NodeID(), "", std::time(nullptr), false, SharePermissions{1, 1, 1, 1, 0, 1, 1, 0, 1});
}

static std::string GetInternalFilename(NodeID const &ID, NodeID const &Change)
	{ return String() << ID.Instance << "-" << ID.Index << "-" << Change.Instance << "-" << Change.Index; }

ShareCore::ShareCore(bfs::path const &Root, std::string const &InstanceName) :
	Root(Root), FilePath(Root / "." App / "files"),
	InstanceName(InstanceName),
	SplitFile(NodeID(), NodeID(), NodeID(), SplitDir, false, 0, SharePermissions{1, 1, 1, 1, 1, 1, 1, 1, 1}, false)
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
		bfs::path const DatabasePath = Root / "." App / "database";

		bfs::path const TransactionPath = Root / "." App / "transactions";

		if (!bfs::exists(Root))
		{
			// Try to create an empty share, since the target didn't exist
			// --
			if (InstanceName.empty())
				{ throw UserError() << "Share '" << Root.string() << "' does not exist.  Specify NAME to create a new share."; }
			if (!ValidateFilename(InstanceName))
				{ throw UserError() << "Instance NAME contains invalid characters."; }

			auto RandomGenerator = std::mt19937_64{std::random_device{}()};
			InstanceID = std::uniform_int_distribution<UUID>()(RandomGenerator);

			InstanceFilename = GetInstanceFilename(InstanceName, InstanceID);

			// Prepare the base file hierarchy
			bfs::create_directory(Root);
			Log.reset(new FileLog(Root / "log.txt"));
			bfs::create_directory(Root / "." App);
			bfs::create_directory(FilePath);
			bfs::create_directory(TransactionPath);

			{
				bfs::path StaticDataPath = Root / "." App / "static";
				bfs::ofstream Out(StaticDataPath, std::ofstream::out | std::ofstream::binary);
				if (!Out) throw SystemError() << "Could not create file '" << StaticDataPath << "'.";
				auto const &Data = StaticDataV1All::Write(InstanceName, InstanceID);
				Out.write((char const *)&Data[0], Data.size());
			}

			{
				bfs::path ReadmePath = Root / App "-share-readme.txt";
				bfs::ofstream Out(ReadmePath);
				if (!Out) throw SystemError() << "Could not create file '" << ReadmePath << "'.";
				Out << "Do not modify the contents of this directory.\n\nThis directory is the unmounted data for a " App " share.  Modifying the contents could cause data corruption.  It is safe to move and change the ownership for this folder (but not it's permissions or contents)." << std::endl;
			}

			Database.reset(new CoreDatabase(DatabasePath, true, InstanceName, InstanceID));
		}
		else if (bfs::is_directory(Root))
		{
			Log.reset(new FileLog(Root / "log.txt"));

			// Share exists, restore state
			// --
			if (!InstanceName.empty())
				Log->Warn() << "Share exists, ignoring all other arguments.";

			// Load static data
			bfs::ifstream In(Root / "." App / "static", std::ifstream::in | std::ifstream::binary);
			Protocol::Reader<FileLog, StaticDataV1All> Reader(*Log,
				[&](std::string const &ReadInstanceName, UUID const &ReadInstanceUUID)
				{
					this->InstanceName = ReadInstanceName;
					this->InstanceID = ReadInstanceUUID;
				});
			bool Success = Reader.Read(In);
			if (!Success)
				throw SystemError() << "Could not read static data, file may be corrupt.";

			InstanceFilename = GetInstanceFilename(InstanceName, InstanceID);

			Database.reset(new CoreDatabase(DatabasePath, false, InstanceName, InstanceID));
			auto MaybeVersion = Database->Get<unsigned int()>("SELECT \"Version\" FROM \"Stats\"");
			if (!MaybeVersion)
				throw SystemError() << "Could not read database version, database may be corrupt.";
			DatabaseVersion Version = (DatabaseVersion)*MaybeVersion;
			switch (Version)
			{
				default: throw SystemError() << "Unrecognized database version " << (unsigned int)Version;
				/*case Last - 2...: Do upgrade to Last - 1 && log;
				case Last - 2...: Do upgrade to Last && log;*/
				case DatabaseVersion::Latest: break;
			}
			Database->Execute("UPDATE \"Stats\" SET \"Version\" = ?", (unsigned int)DatabaseVersion::Latest);
		}
		else { throw UserError() << Root << " is a non-directory.  The root path must not exist or must have been previously created by " << App << "."; }

		Transaction.Create = [this](
			UUID const &FileIndex,
			NodeID const &Parent, std::string const &Name, bool const &IsFile,
			SharePermissions const &Permissions)
		{
			uint64_t Time = std::time(nullptr);
			Database->CreateFile({HostInstanceIndex, FileIndex}, Parent, Name, Time, IsFile, Permissions);
			if (IsFile)
			{
				auto const InternalFilename = FilePath / GetInternalFilename(
					{HostInstanceIndex, FileIndex},
					{HostInstanceIndex, NullIndex});
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
			Database->SetPermissions(
				{HostInstanceIndex, NewChangeIndex},
				SharePermissions{
					OwnerRead, OwnerWrite, OwnerExecute,
					GroupRead, GroupWrite, GroupExecute,
					OtherRead, OtherWrite, OtherExecute},
				File.ID(), File.Change());
			Database->CreateChange({HostInstanceIndex, NewChangeIndex}, File.Change());
			if (File.IsFile())
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), {HostInstanceIndex, NewChangeIndex}));
		};

		Transaction.SetTimestamp = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			uint64_t const &Timestamp)
		{
			Database->SetTimestamp(
				{HostInstanceIndex, NewChangeIndex},
				Timestamp,
				File.ID(), File.Change());
			Database->CreateChange({HostInstanceIndex, NewChangeIndex}, File.Change());
			if (File.IsFile())
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), {HostInstanceIndex, NewChangeIndex}));
		};

		Transaction.Delete = [this](ShareFile const &File)
		{
			Database->DeleteFile(File.ID(), File.Change());
			if (File.IsFile())
				bfs::remove(FilePath / GetInternalFilename(File.ID(), File.Change()));
		};

		Transaction.Move = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			NodeID const &Parent,
			std::string const &Name)
		{
			Database->MoveFile(
				{HostInstanceIndex, NewChangeIndex}, Parent, Name,
				File.ID(), File.Change());
			Database->CreateChange({HostInstanceIndex, NewChangeIndex}, File.Change());
			if (File.IsFile())
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), {HostInstanceIndex, NewChangeIndex}));
		};

		Transact.reset(new CoreTransactor(Mutex, TransactionPath,
			Transaction.Create,
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

bfs::path ShareCore::GetRealPath(ShareFile const &File) const
{
	assert(File.IsFile());
	return FilePath / GetInternalFilename(File.ID(), File.Change());
}

GetResult ShareCore::Get(NodeID const &ID)
{
	// Get primary instance of file by file id
	auto Got = Database->GetFileByID(ID);
	if (!Got) return ActionError::Missing;
	return {*Got};
}

GetResult ShareCore::Get(bfs::path const &Path)
{
	auto ParentFile = Database->GetFile({0, 0}, "");
	assert(ParentFile);
	if (Path.empty()) return {ShareFile(*ParentFile)};

	bfs::path::iterator PathIterator = Path.begin();
	Counter SplitInstance = 0;
	if (*PathIterator == SplitDir)
	{
		if (++PathIterator == Path.end()) return SplitFile;
		auto GotInstance = Database->GetInstanceIndex(PathIterator->string());
		if (!GotInstance) return ActionError::Missing;
		SplitInstance = *GotInstance;
		return ActionError::Missing;
		if (true /*!HasSplits(SplitInstance, ParentFile)*/) return ActionError::Missing;
		if (++PathIterator == Path.end()) return SplitInstanceFile(SplitInstance);
	}

	{
		bfs::path::iterator NextPathIterator = PathIterator; ++NextPathIterator;
		for (; NextPathIterator != Path.end(); PathIterator = NextPathIterator, NextPathIterator++)
		{
			ParentFile = Database->GetFile(ShareFile(*ParentFile).ID(), PathIterator->string());
			if (!ParentFile)
				return ActionError::Missing;
			if (ShareFile(*ParentFile).IsFile())
				return ActionError::Invalid;
			if (true /*!HasSplits(SplitInstance, ParentFile)*/)
				return ActionError::Missing;
		}
	}

	auto Out = (SplitInstance == 0) ?
		Database->GetFile(ShareFile(*ParentFile).ID(), PathIterator->string()) :
		Database->GetSplitFile(ShareFile(*ParentFile).ID(), SplitInstance, PathIterator->string());
	if (!Out) return ActionError::Missing;
	return ShareFile(*Out);
}

std::vector<ShareFile> ShareCore::GetDirectory(ShareFile const &File, unsigned int From, unsigned int Count)
{
	std::vector<ShareFile> Out;
	if (File.IsSplit()) Database->GetSplitFiles.Execute(File.ID(), File.Change().Instance, From, Count,
		[&Out](
			NodeID &&ID, NodeID &&Change, NodeID &&Parent,
			std::string &&Name, bool &&IsFile, Timestamp &&Modified,
			SharePermissions &&Permissions, bool &&IsSplit)
			{ Out.push_back(ShareFile{ID, Change, Parent, Name, IsFile, Modified, Permissions, IsSplit}); });
	else Database->GetFiles.Execute(File.ID(), From, Count,
		[&Out](
			NodeID &&ID, NodeID &&Change, NodeID &&Parent,
			std::string &&Name, bool &&IsFile, Timestamp &&Modified,
			SharePermissions &&Permissions, bool &&IsSplit)
			{ Out.push_back(ShareFile{ID, Change, Parent, Name, IsFile, Modified, Permissions, IsSplit}); });
	assert(Out.size() <= Count);
	return Out;
}

ActionResult<ShareFile> ShareCore::Create(bfs::path const &Path, bool IsFile,
	bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
	bool GroupRead, bool GroupWrite, bool GroupExecute,
	bool OtherRead, bool OtherWrite, bool OtherExecute)
{
	Database->Begin();
	uint64_t FileIndex = Database->GetFileIndex();
	Database->IncrementFileIndex();
	Database->End();
	auto Parent = Get(Path.parent_path());
	if (!Parent) return ActionError::Missing;
	if (Parent->IsFile()) return ActionError::Invalid;
	ShareFile Out(
		{HostInstanceIndex, FileIndex}, Parent->ID(), {},
		Path.filename().string(), IsFile, std::time(nullptr),
		{
			OwnerRead, OwnerWrite, OwnerExecute,
			GroupRead, GroupWrite, GroupExecute,
			OtherRead, OtherWrite, OtherExecute
		},
		false);
	(*Transact)(CTV1Create(), Out.ID().Index, Out.Parent(), Out.Name(), Out.IsFile(), Out.Permissions());
	Log->Debug() << "Created file " << HostInstanceIndex << " " << FileIndex << " / 0 0";
	return Out;
}

void ShareCore::SetPermissions(ShareFile const &File,
	bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
	bool GroupRead, bool GroupWrite, bool GroupExecute,
	bool OtherRead, bool OtherWrite, bool OtherExecute)
{
	Database->Begin();
	uint64_t ChangeIndex = Database->GetChangeIndex();
	Database->IncrementChangeIndex();
	Database->End();
	(*Transact)(CTV1SetPermissions(),
		File, ChangeIndex,
		OwnerRead, OwnerWrite, OwnerExecute,
		GroupRead, GroupWrite, GroupExecute,
		OtherRead, OtherWrite, OtherExecute);
	Log->Debug() << "Changed file " << File.ID().Instance << " " << File.ID().Index << " / " <<
		File.Change().Instance << " " << File.Change().Index << " -> " << HostInstanceIndex << " " << ChangeIndex;
}

void ShareCore::SetTimestamp(ShareFile const &File, unsigned int Timestamp)
{
	Database->Begin();
	uint64_t ChangeIndex = Database->GetChangeIndex();
	Database->IncrementChangeIndex();
	Database->End();
	(*Transact)(CTV1SetTimestamp(),
		File, ChangeIndex,
		Timestamp);
	Log->Debug() << "Changed file " << File.ID().Instance << " " << File.ID().Index << " / " <<
		File.Change().Instance << " " << File.Change().Index << " -> " << HostInstanceIndex << " " << ChangeIndex;
}

void ShareCore::Delete(ShareFile const &File)
{
	(*Transact)(CTV1Delete(), File);
	Log->Debug() << "Deleted file head " << File.ID().Instance << " " << File.ID().Index;
}

ActionError ShareCore::Move(ShareFile const &File, bfs::path const &To)
{
	Database->Begin();
	UUID ChangeIndex = Database->GetChangeIndex();
	Database->IncrementChangeIndex();
	Database->End();

	auto ToFile = Get(To);
	if (!ToFile)
	{
		ToFile = Get(To.parent_path().string());
		if (!ToFile) return ActionError::Missing;
		if (ToFile->IsFile()) return ActionError::Invalid;
	}

	if (ToFile->IsFile())
	{
		(*Transact)(CTV1Move(), File, ChangeIndex, ToFile->Parent(), ToFile->Name());
		Delete(ToFile);
	}
	else (*Transact)(CTV1Move(), File, ChangeIndex, ToFile->ID(), File.Name());
	Log->Debug() << "Changed file " << File.ID().Instance << " " << File.ID().Index << " / " <<
		File.Change().Instance << " " << File.Change().Index << " -> " << HostInstanceIndex << " " << ChangeIndex;
	return ActionError::OK;
}

ShareFile ShareCore::SplitInstanceFile(Counter Index)
{
	return std::forward_as_tuple(NodeID(), NodeID(), NodeID(), String() << Index, false, 0, SharePermissions{1, 1, 1, 1, 1, 1, 1, 1, 1}, false);
}
