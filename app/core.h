#ifndef core_h
#define core_h

// TODO
// Add IsSplit to file (not at all replicated)
// Finish get functions
// FAILPOINT macros, testing utility file - indicate when hit for test verification
// PAUSEPOINT macros (by file/line, pauses until admin command, indicate when hit)
// Test transactions
// Test database
// Add unix domain socket to core, commands, admin app
// Test core
// 

struct ActionError
{
	enum CodeType
	{
		OK,
		Exists,
		Missing,
		Invalid
	} Code;
};

template <typename ValueType> struct ActionResult
{
	ActionResult(ActionError::CodeType Code) : Code(Code) { assert(Code != ActionError::OK); }
	ActionResult(ValueType const &Value) : Code(ActionError::OK), Value(Value) { }
	operator ResultType() { assert(Code == ActionError::OK); return Value; }
	ActionError::CodeType Code;
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
static_assert(sizeof(SharePermissions) == 2, "MiscFileData is unexpectedly not 2 bytes.");

typedef std::tuple<NodeID, NodeID, NodeID, std::string, bool, uint64_t, SharePermissions, bool> ShareFileTuple;
struct ShareFile : ShareFileTuple
{
	NodeID &ID(void) { return std::get<0>(*this); }
	NodeID &Parent(void) { return std::get<1>(*this); }
	NodeID &Change(void) { return std::get<2>(*this); }
	std::string &Name(void) { return std::get<3>(*this); }
	bool &IsFile(void) { return std::get<4>(*this); }
	uint64_t &Timestamp(void) { return std::get<5>(*this); }
	SharePermissions &Permissions(void) { return std::get<6>(*this); }
	bool &IsSplit(void) { return std::get<7>(*this); }

	bool OwnerRead(void) const { return Permissions().OwnerRead; }
	bool OwnerWrite(void) const { return Permissions().OwnerWrite; };
	bool OwnerExecute(void) const { return Permissions().OwnerExecute; };
	bool GroupRead(void) const { return Permissions().GroupRead; };
	bool GroupWrite(void) const { return Permissions().GroupWrite; };
	bool GroupExecute(void) const { return Permissions().GroupExecute; };
	bool OtherRead(void) const { return Permissions().OtherRead; };
	bool OtherWrite(void) const { return Permissions().OtherWrite; };
	bool OtherExecute(void) const { return Permissions().OtherExecute; };
};

struct CoreDatabase : SQLDatabase
{
	CoreDatabase(bfs::path const &DatabasePath, bool Create, std::string const &InstanceName, UUID const &InstanceID);

	std::unique_ptr<SQLDatabase::Statement<void(void)> Begin;
	std::unique_ptr<SQLDatabase::Statement<void(void)> End;
	std::unique_ptr<SQLDatabase::Statement<UUID(void)>> GetFileIndex;
	std::unique_ptr<SQLDatabase::Statement<void(void)> IncrementFileIndex;
	std::unique_ptr<SQLDatabase::Statement<UUID(void)> GetChangeIndex;
	std::unique_ptr<SQLDatabase::Statement<void(void)> IncrementChangeIndex;
	std::unique_ptr<SQLDatabase::Statement<ShareFileTuple(Counter IDInstance, UUID IDIndex)>> GetFileByID;
	std::unique_ptr<SQLDatabase::Statement<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, std::string Name)>> GetFile;
	std::unique_ptr<SQLDatabase::Statement<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter SplitInstance, std::string Name)>> GetSplitFile;
	std::unique_ptr<SQLDatabase::Statement<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter Offset, Counter Limit)>> GetFiles;
	std::unique_ptr<SQLDatabase::Statement<ShareFileTuple(Counter ParentInstance, UUID ParentIndex, Counter SplitInstance, Counter Offset, Counter Limit)>> GetSplitFiles;
	std::unique_ptr<SQLDatabase::Statement<void(Counter, UUID, Counter, UUID, std::string, bool, Timestamp, SharePermissions)>> CreateFile;
	std::unique_ptr<SQLDatabase::Statement<void(Counter, UUID, Counter, UUID)> DeleteFile;
	std::unique_ptr<SQLDatabase::Statement<void(SharePermissions, Counter, UUID, Counter, UUID, Counter, UUID)>> SetPermissions;
	std::unique_ptr<SQLDatabase::Statement<void(Counter, UUID, Timestamp, Counter, UUID, Counter, UUID)>> SetTimestamp;
};

struct ShareCore
{
	ShareCore(bfs::path const &Root, std::string const &InstanceName = std::string());

	bfs::path GetRoot(void) const;

	unsigned int GetUser(void) const;
	unsigned int GetGroup(void) const;

	std::unique_ptr<ShareFile> Create(bfs::path const &Path, bool IsFile, 
		bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
		bool GroupRead, bool GroupWrite, bool GroupExecute,
		bool OtherRead, bool OtherWrite, bool OtherExecute);
	std::unique_ptr<File> Get(bfs::path const &Path);
	std::vector<std::unique_ptr<ShareFile>> GetDirectory(ShareFile const &File, unsigned int From, unsigned int Count);
	void SetPermissions(ShareFile const &File, 
		bool OwnerRead, bool OwnerWrite, bool OwnerExecute,
		bool GroupRead, bool GroupWrite, bool GroupExecute,
		bool OtherRead, bool OtherWrite, bool OtherExecute);
	void SetTimestamp(ShareFile const &File, unsigned int Timestamp);
	void Delete(ShareFile const &File);
	void Move(ShareFile const &File, bfs::path const &To)
	
	private:
		bfs::path Root;
		bfs::path FilePath;

		std::string InstanceName;
		UUID InstanceID;
		Counter InstanceIndex;
		std::string InstanceFilename;

		std::unique_ptr<SQLDatabase> Database;

		struct
		{
			std::function<void(UUID const &ID, 
				NodeID const &Parent, std::string const &Name, bool const &IsFile,
				bool const &OwnerRead, bool const &OwnerWrite, bool const &OwnerExecute,
				bool const &GroupRead, bool const &GroupWrite, bool const &GroupExecute,
				bool const &OtherRead, bool const &OtherWrite, bool const &OtherExecute)> 
				Create;
			std::function<void(
				ShareFile const &File, UUID const &NewChangeIndex,
				bool const &OwnerRead, bool const &OwnerWrite, bool const &OwnerExecute,
				bool const &GroupRead, bool const &GroupWrite, bool const &GroupExecute,
				bool const &OtherRead, bool const &OtherWrite, bool const &OtherExecute)>
				SetPermissions;
			std::function<void(
				ShareFile const &File, UUID const &NewChangeIndex,
				uint64_t const &Timestamp)>
				SetTimestamp;
			std::function<void(ShareFile const &File)> 
				Delete;
			std::function<void(
				ShareFile const &File, UUID const &NewChangeIndex,
				NodeID const &ParentID,
				std::string const &Name)>
				Move;
		} Transaction;
		std::unique_ptr<Transactor> Transact;
};

#endif

