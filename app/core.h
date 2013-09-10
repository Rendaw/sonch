#ifndef core_h
#define core_h

#include "database.h"
#include "transaction.h"

// TODO
// Test fuse different user access, group permissions
// Test mkdir in non-dir path, does mkdir/create get called?
// FAILPOINT macros, testing utility file - indicate when hit for test verification
// PAUSEPOINT macros (by file/line, pauses until admin command, indicate when hit)
// Add unix domain socket to core, commands, admin app
// Test core
//

#define App "sonch"

enum class ActionError
{
	OK,
	Illegal,
	Unknown,
	Exists,
	Missing,
	Invalid,
	Restricted
};

template <typename ValueType> struct ActionResult
{
	ActionResult(ActionError Code) : Code(Code) { assert(Code != ActionError::OK); }
	template <typename InitValueType> ActionResult(InitValueType const &Value) : Code(ActionError::OK), Value(Value) { }
	template <typename InitValueType> ActionResult(InitValueType &&Value) : Code(ActionError::OK), Value(Value) { }
	operator bool(void) const { return Code == ActionError::OK; }
	operator ValueType(void) { assert(Code == ActionError::OK); return Value; }
	ValueType &operator *(void) { return Value; }
	ValueType const &operator *(void) const { return Value; }
	ValueType *operator ->(void) { return &Value; }
	ValueType const *operator ->(void) const { return &Value; }
	ActionError Code;
	ValueType Value;
};

typedef StrictType(uint64_t) UUID;
typedef StrictType(uint64_t) Counter;
typedef StrictType(uint64_t) Timestamp;

struct NodeID
{
	NodeID(void) : Instance((Counter::Type)0), Index((UUID::Type)0) {}
	NodeID(Counter const &Instance, UUID const &Index) : Instance(Instance), Index(Index) {}
	operator bool(void) const
	{
		assert((Instance == (Counter::Type)0) == (Index == (UUID::Type)0));
		return (Instance != (Counter::Type)0) && (Index != (UUID::Type)0);
	}
	Counter Instance;
	UUID Index;
};

#include "shared.h"
struct SharePermissions
{
	unsigned CanWrite : 1;
	unsigned CanExecute : 1;
};

typedef std::tuple<NodeID, NodeID, NodeID, std::string, bool, Timestamp, SharePermissions, bool> ShareFileTuple;
struct ShareFile : ShareFileTuple
{
	using ShareFileTuple::ShareFileTuple;

	inline NodeID const &ID(void) const { return std::get<0>(*this); }
	inline NodeID const &Change(void) const { return std::get<1>(*this); }
	inline NodeID const &Parent(void) const { return std::get<2>(*this); }
	inline std::string const &Name(void) const { return std::get<3>(*this); }
	inline bool const &IsFile(void) const { return std::get<4>(*this); }
	inline Timestamp const &ModifiedTime(void) const { return std::get<5>(*this); }
	inline SharePermissions const &Permissions(void) const { return std::get<6>(*this); }
	inline bool const &IsSplit(void) const { return std::get<7>(*this); }

	inline bool CanWrite(void) const { return Permissions().CanWrite; }
	inline bool CanExecute(void) const { return Permissions().CanExecute; }
};

typedef ActionResult<ShareFile> GetResult;

inline size_t ProtocolGetSize(ShareFile const &Argument) { return sizeof(Argument); }
inline void ProtocolWrite(uint8_t *&Out, ShareFile const &Argument)
{
	memcpy(Out, &Argument, sizeof(Argument));
	Out += sizeof(Argument);
}
template <typename LogType> bool ProtocolRead(LogType &Log, Protocol::VersionIDType const &VersionID, Protocol::MessageIDType const &MessageID, Protocol::BufferType const &Buffer, Protocol::SizeType &Offset, ShareFile &Data)
{
	if (Buffer.size() < StrictCast(Offset, size_t) + sizeof(Data))
	{
		Log.Debug() << "ShareFilend of file reached prematurely reading message body ShareFile size " << sizeof(Data) << ", message length doesn't match contents (version " << *VersionID << ", type " << *MessageID << ")";
		assert(false);
		return false;
	}

	Data = *reinterpret_cast<ShareFile const *>(&Buffer[*Offset]);
	Offset += (Protocol::SizeType::Type)sizeof(Data);
	return true;
}

inline size_t ProtocolGetSize(NodeID const &Argument) { return sizeof(Argument); }
inline void ProtocolWrite(uint8_t *&Out, NodeID const &Argument)
{
	memcpy(Out, &Argument, sizeof(Argument));
	Out += sizeof(Argument);
}
template <typename LogType> bool ProtocolRead(LogType &Log, Protocol::VersionIDType const &VersionID, Protocol::MessageIDType const &MessageID, Protocol::BufferType const &Buffer, Protocol::SizeType &Offset, NodeID &Data)
{
	if (Buffer.size() < StrictCast(Offset, size_t) + sizeof(Data))
	{
		Log.Debug() << "NodeIDnd of file reached prematurely reading message body NodeID size " << sizeof(Data) << ", message length doesn't match contents (version " << *VersionID << ", type " << *MessageID << ")";
		assert(false);
		return false;
	}

	Data = *reinterpret_cast<NodeID const *>(&Buffer[*Offset]);
	Offset += (Protocol::SizeType::Type)sizeof(Data);
	return true;
}

inline size_t ProtocolGetSize(SharePermissions const &Argument) { return sizeof(Argument); }
inline void ProtocolWrite(uint8_t *&Out, SharePermissions const &Argument)
{
	memcpy(Out, &Argument, sizeof(Argument));
	Out += sizeof(Argument);
}
template <typename LogType> bool ProtocolRead(LogType &Log, Protocol::VersionIDType const &VersionID, Protocol::MessageIDType const &MessageID, Protocol::BufferType const &Buffer, Protocol::SizeType &Offset, SharePermissions &Data)
{
	if (Buffer.size() < StrictCast(Offset, size_t) + sizeof(Data))
	{
		Log.Debug() << "SharePermissionsnd of file reached prematurely reading message body SharePermissions size " << sizeof(Data) << ", message length doesn't match contents (version " << *VersionID << ", type " << *MessageID << ")";
		assert(false);
		return false;
	}

	Data = *reinterpret_cast<SharePermissions const *>(&Buffer[*Offset]);
	Offset += (Protocol::SizeType::Type)sizeof(Data);
	return true;
}

struct CoreDatabaseOperations
{
	void Bind(sqlite3 *BareContext, sqlite3_stmt *Context, char const *Template, int &Index, NodeID const &Value)
	{
		if (sqlite3_bind_int64(Context, Index, *reinterpret_cast<int64_t const *>(&Value.Instance.Value)) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BareContext);
		if (sqlite3_bind_int64(Context, Index + 1, *reinterpret_cast<int64_t const *>(&Value.Index.Value)) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BareContext);
		Index += 2;
	}

	NodeID Unbind(sqlite3_stmt *Context, int &Index, ::Type<NodeID>)
	{
		int const BaseIndex = Index;
		Index += 2;
		return
		{
			static_cast<Counter::Type>(sqlite3_column_int(Context, BaseIndex)),
			static_cast<UUID::Type>(sqlite3_column_int(Context, BaseIndex + 1))
		};
	}

	void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, char const *Template, int &Index, SharePermissions const &Value)
	{
		if (sqlite3_bind_blob(Context, Index, &Value, sizeof(Value), nullptr) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		++Index;
	}

	SharePermissions Unbind(sqlite3_stmt *Context, int &Index, ::Type<SharePermissions>)
	{
		assert(sqlite3_column_bytes(Context, Index) == sizeof(SharePermissions));
		return *static_cast<SharePermissions const *>(sqlite3_column_blob(Context, Index++));
	}

};
struct CoreDatabaseStructure : SQLDatabase<CoreDatabaseOperations>
{
	CoreDatabaseStructure(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID);
};
struct CoreDatabase : CoreDatabaseStructure
{
	CoreDatabase(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID);

	template <typename Signature> using Statement = typename SQLDatabase<CoreDatabaseOperations>::Statement<Signature>;

	Statement<void(void)> Begin;
	Statement<void(void)> End;
	Statement<UUID(void)> GetFileIndex;
	Statement<void(void)> IncrementFileIndex;
	Statement<UUID(void)> GetChangeIndex;
	Statement<void(void)> IncrementChangeIndex;
	Statement<Counter(std::string Filename)> GetInstanceIndex;
	Statement<ShareFileTuple(NodeID ID)> GetFileByID;
	Statement<ShareFileTuple(NodeID Parent, std::string Name)> GetFile;
	Statement<ShareFileTuple(NodeID Parent, Counter SplitInstance, std::string Name)> GetSplitFile;
	Statement<ShareFileTuple(NodeID Parent, unsigned int Offset, unsigned int Limit)> GetFiles;
	Statement<ShareFileTuple(NodeID Parent, Counter SplitInstance, unsigned int Offset, unsigned int Limit)> GetSplitFiles;
	Statement<void(NodeID ID, NodeID Parent, std::string Name, bool IsFile, Timestamp ModifiedTime, SharePermissions Permissions)> CreateFile;
	Statement<void(NodeID ID, NodeID Change)> DeleteFile;
	Statement<void(NodeID NewChange, SharePermissions NewPermissions, NodeID ID, NodeID Change)> SetPermissions;
	Statement<void(NodeID NewChange, Timestamp NewModifiedTime, NodeID ID, NodeID Change)> SetTimestamp;
	Statement<void(NodeID NewChange, NodeID NewParent, std::string NewName, NodeID ID, NodeID Change)> MoveFile;
	Statement<void(NodeID NewChange, NodeID OldChange)> CreateChange;
	Statement<NodeID(NodeID Change)> GetChange;
};

DefineProtocol(CoreTransactorProtocol)
DefineProtocolVersion(CoreTransactorVersion1, CoreTransactorProtocol)
DefineProtocolMessage(CTV1Create, CoreTransactorVersion1,
	void(
		UUID ID,
		NodeID Parent, std::string Name, bool IsFile,
		SharePermissions Permissions))
DefineProtocolMessage(CTV1SetPermissions, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		bool CanWrite, bool CanExecute))
DefineProtocolMessage(CTV1SetTimestamp, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		Timestamp NewTimestamp))
DefineProtocolMessage(CTV1Delete, CoreTransactorVersion1,
	void(ShareFile File))
DefineProtocolMessage(CTV1Move, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		NodeID ParentID,
		std::string Name))

struct ShareCore
{
	ShareCore(bfs::path const &Root, std::string const &InstanceName = std::string());

	bfs::path GetRoot(void) const;

	bfs::path GetRealPath(ShareFile const &File) const;

	GetResult Get(bfs::path const &Path);

	ActionError CreateDirectory(bfs::path const &Path, bool CanWrite, bool CanExecute);
	//ActionResult<Optional<OpenFileContext>> CreateFile(bfs::path const &Path, bool CanWrite, bool CanExecute, bool Open);
	ActionResult<std::unique_ptr<ShareFile>> OpenDirectory(bfs::path const &Path);
	std::vector<ShareFile> GetDirectory(ShareFile const &File, unsigned int From, unsigned int Count);
	ActionError SetPermissions(bfs::path const &Path, bool CanWrite, bool CanExecute);
	ActionError SetTimestamp(bfs::path const &Path, Timestamp const &NewTimestamp);
	ActionError Delete(bfs::path const &Path);
	ActionError Move(bfs::path const &From, bfs::path const &To);

	private:
		//GetResult Get(NodeID const &ID);
		GetResult GetInternal(bfs::path const &Path);
		NodeID GetPrecedingChange(NodeID const &Change);

		bool IsRootPath(bfs::path const &Path) const;
		bool IsSplitPath(bfs::path const &Path) const;
		ShareFile SplitInstanceFile(Counter Index) const;

		bfs::path const Root;
		bfs::path const FilePath;

		std::unique_ptr<FileLog> Log;

		std::string InstanceName;
		UUID InstanceID;
		std::string InstanceFilename;

		std::mutex Mutex;

		ShareFile SplitFile;

		std::unique_ptr<CoreDatabase> Database;

		typedef Transactor<CTV1Create, CTV1SetPermissions, CTV1SetTimestamp, CTV1Delete, CTV1Move> CoreTransactor;
		struct
		{
			CTV1Create::Function Create;
			CTV1SetPermissions::Function SetPermissions;
			CTV1SetTimestamp::Function SetTimestamp;
			CTV1Delete::Function Delete;
			CTV1Move::Function Move;
		} Transaction;
		std::unique_ptr<CoreTransactor> Transact;
};

#endif

