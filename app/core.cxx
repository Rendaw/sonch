#include "core.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#define SplitDir "splits"
#define HostInstanceIndex static_cast<Counter::Type>(0)
#define NullIndex static_cast<UUID::Type>(0)

DefineProtocol(StaticDataProtocol)
DefineProtocolVersion(StaticDataV1, StaticDataProtocol)
DefineProtocolMessage(StaticDataV1All, StaticDataV1, void(std::string InstanceName, UUID InstanceUUID))

enum class DatabaseVersion : unsigned int
{
	V1 = 0,
	End,
	Latest = End - 1
};

CoreDatabaseStructure::CoreDatabaseStructure(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID) : SQLDatabase<CoreDatabaseOperations>(DatabasePath)
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
		Execute("INSERT INTO \"Instances\" (\"ID\", \"Name\", \"Filename\") VALUES (?, ?, ?)", InstanceID, InstanceName, InstanceName);

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
	GetFiles(Prepare<ShareFileTuple(NodeID Parent, unsigned int Offset, unsigned int Limit)>
		("SELECT * FROM \"Files\" WHERE \"ParentInstance\" = ? AND \"ParentIndex\" = ? AND \"IsSplit\" = 0 LIMIT ?, ?")),
	GetSplitFiles(Prepare<ShareFileTuple(NodeID Parent, Counter SplitInstance, unsigned int Offset, unsigned int Limit)>
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
		("INSERT OR IGNORE INTO \"Ancestry\" VALUES (?, ?, ?, ?)")),
	GetChange(Prepare<NodeID(NodeID Change)>
		("SELECT \"ParentInstance\", \"ParentIndex\" FROM \"Ancestry\" WHERE \"IDInstance\" = ? AND \"IDIndex\" = ?"))
{
	if (Create)
		CreateFile(NodeID(), NodeID(), "", false, static_cast<Timestamp::Type>(std::time(nullptr)), SharePermissions{1, 1});
}

static std::string GetInternalFilename(NodeID const &ID, NodeID const &Change)
	{ return String() << *ID.Instance << "-" << *ID.Index << "-" << *Change.Instance << "-" << *Change.Index; }

void ValidatePath(bfs::path const &Path) { Assert(*Path.begin() == "/"); }

ShareCoreInner::ShareCoreInner(bfs::path const &Root, std::string const &InstanceName) :
	Root(Root), FilePath(Root / "." App / "files"),
	InstanceName(InstanceName),
	SplitFile(NodeID(), NodeID(), NodeID(), SplitDir, false, static_cast<Timestamp::Type>(0), SharePermissions{1, 1}, false)
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
			Buffer[Name.size() + 1 + Offset * 2] = 'a' + static_cast<char>((*(reinterpret_cast<char *>(&ID) + Offset) & 0xF));
			Buffer[Name.size() + 1 + Offset * 2 + 1] = 'a' + static_cast<char>(((*(reinterpret_cast<char *>(&ID) + Offset) & 0xF0) >> 4));
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
			InstanceID = std::uniform_int_distribution<UUID::Type>()(RandomGenerator);

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
				Out.write((char const *)&Data[0], (std::streamsize)Data.size());
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
			Timestamp const Time = static_cast<Timestamp::Type>(std::time(nullptr));
			Database->CreateFile({HostInstanceIndex, FileIndex}, Parent, Name, IsFile, Time, Permissions);
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
			bool const &CanWrite, bool const &CanExecute)
		{
			Database->SetPermissions(
				{HostInstanceIndex, NewChangeIndex},
				SharePermissions{CanWrite, CanExecute},
				File.ID(), File.Change());
			Database->CreateChange({HostInstanceIndex, NewChangeIndex}, File.Change());
			if (File.IsFile())
				bfs::rename(FilePath / GetInternalFilename(File.ID(), File.Change()),
					FilePath / GetInternalFilename(File.ID(), {HostInstanceIndex, NewChangeIndex}));
		};

		Transaction.SetTimestamp = [this](
			ShareFile const &File, UUID const &NewChangeIndex,
			Timestamp const &NewTimestamp)
		{
			Database->SetTimestamp(
				{HostInstanceIndex, NewChangeIndex},
				NewTimestamp,
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

		Transact.reset(new CoreTransactor(TransactionPath,
			Transaction.Create,
			Transaction.SetPermissions,
			Transaction.SetTimestamp,
			Transaction.Delete,
			Transaction.Move));
	}
	catch (bfs::filesystem_error &Error)
		{ throw SystemError() << Error.what(); }
}

bfs::path ShareCoreInner::GetRoot(void) const { return Root; }

bfs::path ShareCoreInner::GetRealPath(ShareFile const &File) const
{
	Assert(File.IsFile());
	return FilePath / GetInternalFilename(File.ID(), File.Change());
}

GetResult ShareCoreInner::Get(bfs::path const &Path)
{
	ValidatePath(Path);
#ifndef NDEBUG
	Assert(Mutex.try_lock());
	Mutex.unlock();
#endif
	std::lock_guard<std::mutex> Guard(Mutex);
	return GetInternal(Path);
}

ActionError ShareCoreInner::CreateDirectory(bfs::path const &Path, bool CanWrite, bool CanExecute)
{
	ValidatePath(Path);
	std::lock_guard<std::mutex> Guard(Mutex);
	auto Parent = GetInternal(Path.parent_path());
	if (IsSplitPath(Path)) return ActionError::Illegal;
	if (!Parent) return ActionError::Missing;
	if (Parent->IsFile()) return ActionError::Invalid;
	if (Database->GetFile(Parent->ID(), Path.filename().string()))
		return ActionError::Exists;
	Database->Begin();
	UUID FileIndex = *Database->GetFileIndex();
	Database->IncrementFileIndex();
	Database->End();
	(*Transact)(CTV1Create(), FileIndex, Parent->ID(), Path.filename().string(), false, SharePermissions{CanWrite, CanExecute});
	Log->Debug() << "Created file " << HostInstanceIndex << " " << *FileIndex << " / 0 0";
	return ActionError::OK;
}

ActionResult<std::unique_ptr<ShareFile>> ShareCoreInner::OpenDirectory(bfs::path const &Path)
{
	ValidatePath(Path);
	std::lock_guard<std::mutex> Guard(Mutex);
	auto Out = GetInternal(Path);
	if (!Out) return Out.Code;
	if (!Out->IsFile()) return ActionError::Invalid;
	return new ShareFile(*Out);
}

std::vector<ShareFile> ShareCoreInner::GetDirectory(ShareFile const &File, unsigned int From, unsigned int Count)
{
	std::lock_guard<std::mutex> Guard(Mutex);
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
	Assert(Out.size() <= Count);
	return Out;
}

ActionError ShareCoreInner::SetPermissions(bfs::path const &Path, bool CanWrite, bool CanExecute)
{
	ValidatePath(Path);
	std::lock_guard<std::mutex> Guard(Mutex);
	auto File = GetInternal(Path);
	if (!File) return File.Code;
	Database->Begin();
	UUID ChangeIndex = *Database->GetChangeIndex();
	Database->IncrementChangeIndex();
	Database->End();
	(*Transact)(CTV1SetPermissions(),
		*File, ChangeIndex,
		CanWrite, CanExecute);
	Log->Debug() << "Changed file " << *File->ID().Instance << " " << *File->ID().Index << " / " <<
		*File->Change().Instance << " " << *File->Change().Index << " -> " << HostInstanceIndex << " " << *ChangeIndex;
	return ActionError::OK;
}

ActionError ShareCoreInner::SetTimestamp(bfs::path const &Path, Timestamp const &NewTimestamp)
{
	ValidatePath(Path);
	std::lock_guard<std::mutex> Guard(Mutex);
	auto File = GetInternal(Path);
	if (!File) return File.Code;
	Database->Begin();
	UUID ChangeIndex = *Database->GetChangeIndex();
	Database->IncrementChangeIndex();
	Database->End();
	(*Transact)(CTV1SetTimestamp(),
		*File, ChangeIndex,
		NewTimestamp);
	Log->Debug() << "Changed file " << *File->ID().Instance << " " << *File->ID().Index << " / " <<
		*File->Change().Instance << " " << *File->Change().Index << " -> " << HostInstanceIndex << " " << *ChangeIndex;
	return ActionError::OK;
}

ActionError ShareCoreInner::Delete(bfs::path const &Path)
{
	ValidatePath(Path);
	if (IsRootPath(Path)) return ActionError::Illegal;
	std::lock_guard<std::mutex> Guard(Mutex);
	auto File = GetInternal(Path);
	if (!File) return File.Code;
	if (IsSplitPath(Path) && !File->IsSplit()) return ActionError::Illegal;
	(*Transact)(CTV1Delete(), *File);
	Log->Debug() << "Deleted file head " << *File->ID().Instance << " " << *File->ID().Index;
	return ActionError::OK;
}

ActionError ShareCoreInner::Move(bfs::path const &From, bfs::path const &To)
{
	if (IsRootPath(From)) return ActionError::Illegal;
	if (IsSplitPath(From)) return ActionError::Illegal; // TODO make this okay for non-pseudo, but a copy and delete rather than a reparent
	if (IsSplitPath(To)) return ActionError::Illegal;
	bool DeleteAfter = false;
	{ std::lock_guard<std::mutex> Guard(Mutex);
		Database->Begin();
		UUID ChangeIndex = *Database->GetChangeIndex();
		Database->IncrementChangeIndex();
		Database->End();

		auto FromFile = GetInternal(From);
		if (!FromFile) return FromFile.Code;

		std::string ToName = To.filename().string();
		auto ToFile = GetInternal(To);
		if (ToFile.Code == ActionError::Missing)
			ToFile = GetInternal(To.parent_path().string());
		else if (ToFile && !ToFile->IsFile())
			ToName = From.filename().string();

		if (!ToFile) return ToFile.Code;
		if (!ToFile->CanWrite()) return ActionError::Restricted;

		if (ToFile->IsFile())
		{
			(*Transact)(CTV1Move(), *FromFile, ChangeIndex, ToFile->Parent(), ToName);
			DeleteAfter = true;
		}
		else (*Transact)(CTV1Move(), *FromFile, ChangeIndex, ToFile->ID(), ToName);
		Log->Debug() << "Changed file " << *FromFile->ID().Instance << " " << *FromFile->ID().Index << " / " <<
			*FromFile->Change().Instance << " " << *FromFile->Change().Index << " -> " << HostInstanceIndex << " " << *ChangeIndex;
	}
	if (DeleteAfter) Delete(To);
	return ActionError::OK;
}

/*GetResult ShareCoreInner::Get(NodeID const &ID)
{
	// Get primary instance of file by file id
	auto Got = Database->GetFileByID(ID);
	if (!Got) return ActionError::Missing;
	return {*Got};
}*/

GetResult ShareCoreInner::GetInternal(bfs::path const &Path)
{
	ValidatePath(Path);
	Assert(!Mutex.try_lock());
	auto ParentFile = Database->GetFile({HostInstanceIndex, NullIndex}, "");
	Assert(ParentFile);
	bfs::path::iterator PathIterator = ++Path.begin();
	if (IsRootPath(Path)) return {ShareFile(*ParentFile)};

	Counter SplitInstance = HostInstanceIndex;
	bool const IsSplit = IsSplitPath(Path);
	if (IsSplit)
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
			auto OriginalParentFile = ParentFile;
			if (IsSplit)
				ParentFile = Database->GetSplitFile(ShareFile(*OriginalParentFile).ID(), SplitInstance, PathIterator->string());
			if (!IsSplit || !ParentFile)
				ParentFile = Database->GetFile(ShareFile(*OriginalParentFile).ID(), PathIterator->string());
			if (!ParentFile)
				return ActionError::Missing;
			if (ShareFile(*ParentFile).IsFile())
				return ActionError::Invalid;
		}
	}

	auto Out = (!IsSplit) ?
		Database->GetFile(ShareFile(*ParentFile).ID(), PathIterator->string()) :
		Database->GetSplitFile(ShareFile(*ParentFile).ID(), SplitInstance, PathIterator->string());
	if (!Out) return ActionError::Missing;
	return ShareFile(*Out);
}

NodeID ShareCoreInner::GetPrecedingChange(NodeID const &Change)
{
	if (!Change) return NodeID();
	std::lock_guard<std::mutex> Guard(Mutex);
	auto Out = Database->GetChange(Change);
	Assert(Out);
	return *Out;
}

bool ShareCoreInner::IsRootPath(bfs::path const &Path) const
{
	bfs::path::iterator PathIterator = ++Path.begin();
	return (PathIterator == Path.end());
}

bool ShareCoreInner::IsSplitPath(bfs::path const &Path) const
{
	bfs::path::iterator PathIterator = ++Path.begin();
	if (PathIterator == Path.end()) return false;
	return *PathIterator == SplitDir;
}

ShareFile ShareCoreInner::SplitInstanceFile(Counter Index) const
{
	return std::forward_as_tuple(NodeID(), NodeID(), NodeID(), String() << *Index, false, Timestamp::Type(0), SharePermissions{1, 1}, false);
}
