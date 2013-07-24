#ifndef core_h
#define core_h

#include "shared.h"
struct MiscFileData;
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
	unsigned IsFile : 1;
};
static_assert(sizeof(MiscFileData) == 2, "MiscFileData is unexpectedly not 2 bytes.");

struct ShareFile
{
	std::string Name;
	MiscFileData Misc;
	uint64_t Timestamp;
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
	
	private:
		bfs::path Root;

		std::string InstanceName;
		UUID InstanceID;
		std::string InstanceFilename;

		std::unique_ptr<SQLDatabase> Database;
		struct
		{
			std::unique_ptr<SQLDatabase::Statement> AddFile;
		} Statement;
};

#endif

