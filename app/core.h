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
typedef uint64_t UUID;

enum class ActionError
{
	OK,
	Unknown,
	Exists,
	Missing,
	Invalid
};

template <typename ValueType> struct ActionResult
{
	ActionResult(ActionError Code) : Code(Code) { assert(Code != ActionError::OK); }
	ActionResult(ValueType const &Value) : Code(ActionError::OK), Value(Value) { }
	operator bool(void) { return Code == ActionError::OK; }
	operator ValueType(void) { assert(Code == ActionError::OK); return Value; }
	ValueType &operator *(void) { return Value; }
	ValueType const &operator *(void) const { return Value; }
	ValueType *operator ->(void) { return &Value; }
	ValueType const *operator ->(void) const { return &Value; }
	ActionError Code;
	ValueType Value;
};

typedef uint64_t Counter;
typedef uint64_t Timestamp;

struct NodeID
{
	Counter Instance;
	UUID Index;
};

#include "shared.h"
struct SharePermissions
{
	unsigned OwnerRead : 1;
	unsigned OwnerWrite : 1;
	unsigned OwnerExecute : 1;
	unsigned GroupRead : 1;
	unsigned GroupWrite : 1;
	unsigned GroupExecute : 1;
	unsigned OtherRead : 1;
	unsigned OtherWrite : 1;
	unsigned OtherExecute : 1;
};

typedef std::tuple<NodeID, NodeID, NodeID, std::string, bool, Timestamp, SharePermissions, bool> ShareFileTuple;
struct ShareFile : ShareFileTuple
{
	using ShareFileTuple::ShareFileTuple;

	inline NodeID const &ID(void) const { return std::get<0>(*this); }
	inline NodeID const &Parent(void) const { return std::get<1>(*this); }
	inline NodeID const &Change(void) const { return std::get<2>(*this); }
	inline std::string const &Name(void) const { return std::get<3>(*this); }
	inline bool const &IsFile(void) const { return std::get<4>(*this); }
	inline Timestamp const &ModifiedTime(void) const { return std::get<5>(*this); }
	inline SharePermissions const &Permissions(void) const { return std::get<6>(*this); }
	inline bool const &IsSplit(void) const { return std::get<7>(*this); }

	inline bool OwnerRead(void) const { return Permissions().OwnerRead; }
	inline bool OwnerWrite(void) const { return Permissions().OwnerWrite; }
	inline bool OwnerExecute(void) const { return Permissions().OwnerExecute; }
	inline bool GroupRead(void) const { return Permissions().GroupRead; }
	inline bool GroupWrite(void) const { return Permissions().GroupWrite; }
	inline bool GroupExecute(void) const { return Permissions().GroupExecute; }
	inline bool OtherRead(void) const { return Permissions().OtherRead; }
	inline bool OtherWrite(void) const { return Permissions().OtherWrite; }
	inline bool OtherExecute(void) const { return Permissions().OtherExecute; }
};

typedef ActionResult<ShareFile> GetResult;

struct CoreDatabaseOperations
{
	void Bind(sqlite3 *BareContext, sqlite3_stmt *Context, char const *Template, size_t &Index, NodeID const &Value)
	{
		if (sqlite3_bind_int(Context, Index, Value.Instance) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BareContext);
		if (sqlite3_bind_int(Context, Index + 1, Value.Index) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BareContext);
		Index += 2;
	}

	NodeID Unbind(sqlite3_stmt *Context, size_t &Index, ::Type<NodeID>)
	{
		size_t const BaseIndex = Index;
		Index += 2;
		return
		{
			static_cast<Counter>(sqlite3_column_int(Context, BaseIndex)),
			static_cast<Counter>(sqlite3_column_int(Context, BaseIndex + 1))
		};
	}

	void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, char const *Template, size_t &Index, SharePermissions const &Value)
	{
		if (sqlite3_bind_blob(Context, Index, &Value, sizeof(Value), nullptr) != SQLITE_OK)
			throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		++Index;
	}

	SharePermissions Unbind(sqlite3_stmt *Context, size_t &Index, ::Type<SharePermissions>)
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
	Statement<ShareFileTuple(NodeID Parent, Counter Offset, Counter Limit)> GetFiles;
	Statement<ShareFileTuple(NodeID Parent, Counter SplitInstance, Counter Offset, Counter Limit)> GetSplitFiles;
	Statement<void(NodeID ID, NodeID Parent, std::string Name, bool IsFile, Timestamp ModifiedTime, SharePermissions Permissions)> CreateFile;
	Statement<void(NodeID ID, NodeID Change)> DeleteFile;
	Statement<void(NodeID NewChange, SharePermissions NewPermissions, NodeID ID, NodeID Change)> SetPermissions;
	Statement<void(NodeID NewChange, Timestamp NewModifiedTime, NodeID ID, NodeID Change)> SetTimestamp;
	Statement<void(NodeID NewChange, NodeID NewParent, std::string NewName, NodeID ID, NodeID Change)> MoveFile;
	Statement<void(NodeID NewChange, NodeID OldChange)> CreateChange;
};

DefineProtocol(CoreTransactorProtocol);
DefineProtocolVersion(CoreTransactorVersion1, CoreTransactorProtocol);
DefineProtocolMessage(CTV1Create, CoreTransactorVersion1,
	void(
		UUID ID,
		NodeID Parent, std::string Name, bool IsFile,
		SharePermissions Permissions));
DefineProtocolMessage(CTV1SetPermissions, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
		bool GroupRead, bool GroupWrite, bool GroupExecute,
		bool OtherRead, bool OtherWrite, bool OtherExecute));
DefineProtocolMessage(CTV1SetTimestamp, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		uint64_t Timestamp));
DefineProtocolMessage(CTV1Delete, CoreTransactorVersion1,
	void(ShareFile File));
DefineProtocolMessage(CTV1Move, CoreTransactorVersion1,
	void(
		ShareFile File, UUID NewChangeIndex,
		NodeID ParentID,
		std::string Name));

struct ShareCore
{
	ShareCore(bfs::path const &Root, std::string const &InstanceName = std::string());

	bfs::path GetRoot(void) const;

	unsigned int GetUser(void) const;
	unsigned int GetGroup(void) const;

	bfs::path GetRealPath(ShareFile const &File) const;

	ActionResult<ShareFile> Create(bfs::path const &Path, bool IsFile,
		bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
		bool GroupRead, bool GroupWrite, bool GroupExecute,
		bool OtherRead, bool OtherWrite, bool OtherExecute);
	GetResult Get(NodeID const &ID);
	GetResult Get(bfs::path const &Path);
	std::vector<ShareFile> GetDirectory(ShareFile const &File, unsigned int From, unsigned int Count);
	void SetPermissions(ShareFile const &File,
		bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
		bool GroupRead, bool GroupWrite, bool GroupExecute,
		bool OtherRead, bool OtherWrite, bool OtherExecute);
	void SetTimestamp(ShareFile const &File, unsigned int Timestamp);
	void Delete(ShareFile const &File);
	ActionError Move(ShareFile const &File, bfs::path const &To);

	private:
		bfs::path const Root;
		bfs::path const FilePath;

		std::unique_ptr<FileLog> Log;

		std::string InstanceName;
		UUID InstanceID;
		Counter InstanceIndex;
		std::string InstanceFilename;

		std::mutex Mutex;

		ShareFile SplitInstanceFile(Counter Index);
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

