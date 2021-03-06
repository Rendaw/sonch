#include "log.h"
#include "shared.h"
#include "protocol.h"
#include "core.h"

#include <string>
#include <memory>
#include <cassert>
#include <dirent.h>
#include <sys/time.h>
#include <sstream>

#define _FILE_OFFSET_BITS 64
#define _REENTRANT
#define FUSE_USE_VERSION 26
#include <fuse.h>

struct
{
	bfs::path RootPath;
	std::string InstanceName;
} static PreinitContext;

static std::unique_ptr<ShareCore> Core;

void ExportAttributes(ShareFile const &File, struct stat *Output)
{
	Output->st_mode =
		(File.IsFile() ? S_IFREG : S_IFDIR) |
		S_IRUSR |
		(File.CanWrite() ? S_IWUSR : 0) |
		(File.CanExecute() ? S_IXUSR : 0) |
		S_IRGRP |
		(File.CanWrite() ? S_IWGRP : 0) |
		(File.CanExecute() ? S_IXGRP : 0) |
		S_IROTH |
		(File.CanWrite() ? S_IWOTH : 0) |
		(File.CanExecute() ? S_IXOTH : 0);
	Output->st_nlink = 0;
	Output->st_uid = getuid();
	Output->st_gid = getgid();
	Output->st_mtime = StrictCast(File.ModifiedTime(), time_t);
	Output->st_ctime = StrictCast(File.ModifiedTime(), time_t);
}

/*struct FileContext
{
	std::unique_ptr<ShareFile> Share;
	int FileDescriptor;
	FileContext(void) : FileDescriptor(-1) {}
	~FileContext(void) { if (FileDescriptor >= 0) close(FileDescriptor); }
};*/

int main(int argc, char **argv)
{
	StandardOutLog Log("initialization");

	if (argc <= 2)
	{
		Log.Note() << ("Usage: " App " LOCATION MOUNTPOINT [NAME]\n"
			"\tMounts " App " share LOCATION at MOUNTPOINT.  If LOCATION does not exist, creates a new share with NAME.");
		return 0;
	}

	PreinitContext.RootPath = argv[1];
	if (argc >= 3) PreinitContext.InstanceName = argv[3];

	fuse_operations FuseCallbacks{0};

	// Disabled
	/*FuseCallbacks.link = [](const char *from, const char *to) { return -EPERM; };
	FuseCallbacks.symlink = [](const char *from, const char *to) { return -EPERM; };
	FuseCallbacks.readlink = [](const char *path, char *buf, size_t size) { return -EINVAL; };
	FuseCallbacks.mknod = [](const char *, mode_t mode, dev_t) { return -EPERM; };
	FuseCallbacks.chown = [](const char *path, uid_t uid, gid_t gid) { return -EPERM; };

	FuseCallbacks.setxattr = [](const char *path, const char *name, const char *value, size_t size, int flags)
		{ return -EPERM; };

	FuseCallbacks.getxattr = [](const char *path, const char *name, char *value, size_t size)
		{ return -EPERM; };

	FuseCallbacks.listxattr = [](const char *path, char *list, size_t size)
		{ return -EPERM; };

	FuseCallbacks.removexattr = [](const char *path, const char *name)
		{ return -EPERM; };*/

	// Non-filesystem events
	FuseCallbacks.init = [](fuse_conn_info *conn) -> void *
	{
		StandardOutLog Log("initialization");
		try { Core.reset(new ShareCore(PreinitContext.RootPath, PreinitContext.InstanceName)); }
		catch (UserError &Message)
		{
			Log.Error() << Message;
			exit(1);
		}
		catch (SystemError &Message)
		{
			Log.Error() << "Encountered a system error during initialization.\n\t" << Message;
			exit(1);
		}
		return nullptr;
	};

	// Lookup/read metadata actions
	FuseCallbacks.getattr = [](const char *path, struct stat *stbuf)
	{
		GetResult File = (*Core)->Get(path);
		if (!File) return -ENOENT;
		if (File->IsFile())
		{
			int Result = lstat((*Core)->GetRealPath(*File).string().c_str(), stbuf);
			if (Result == -1) return -errno;
		}
		ExportAttributes(*File, stbuf);
		return 0;
	};

	/*FuseCallbacks.fgetattr = [](const char *, struct stat *stbuf, struct fuse_file_info *fi)
	{
		ShareFile *File = reinterpret_cast<ShareFile *>(fi->fh);
		int Result = fstat(File->FileDescriptor, stbuf);
		if (Result == -1) return -errno;
		return ExportAttributes(*File, stbuf);
	};*/

	FuseCallbacks.access = [](const char *path, int mask)
	{
		GetResult File = (*Core)->Get(path);
		if (!File) return -ENOENT;
		if
		(
			(!(mask & W_OK) || File->CanWrite()) &&
			(!(mask & X_OK) || File->CanExecute())
		) return 0;
		return -EACCES;
	};

	FuseCallbacks.statfs = [](const char *path, struct statvfs *stbuf)
	{
		int Result = statvfs((*Core)->GetRoot().string().c_str(), stbuf);
		if (Result == -1) return -errno;
		return 0;
	};

	// Directory or file changes
	FuseCallbacks.rename = [](const char *from, const char *to)
	{
		switch ((*Core)->Move(from, to))
		{
			case ActionError::OK: break;
			case ActionError::Invalid: return -ENOTDIR;
			case ActionError::Missing: return -ENOENT;
			case ActionError::Restricted: return -EACCES;
			default: assert(false);
		}
		return 0;
	};

	FuseCallbacks.chmod = [](const char *path, mode_t mode)
	{
		switch ((*Core)->SetPermissions(path, mode & S_IWUSR, mode & S_IXUSR))
		{
			case ActionError::OK: break;
			case ActionError::Invalid: return -ENOTDIR;
			case ActionError::Missing: return -ENOENT;
			default: assert(false);
		}
		return 0;
	};

	FuseCallbacks.utimens = [](const char *path, const struct timespec ts[2])
	{
		switch ((*Core)->SetTimestamp(path, static_cast<Timestamp::Type>(ts[1].tv_sec)))
		{
			case ActionError::OK: break;
			case ActionError::Invalid: return -ENOTDIR;
			case ActionError::Missing: return -ENOENT;
			default: assert(false);
		}
		/*struct timeval tv[2];
		tv[0].tv_sec = ts[0].tv_sec;
		tv[0].tv_usec = ts[0].tv_nsec / 1000;
		tv[1].tv_sec = ts[1].tv_sec;
		tv[1].tv_usec = ts[1].tv_nsec / 1000;
		utimes(File->GetRealPath().string().c_str(), tv);*/
		return 0;
	};

	// Directory access
	FuseCallbacks.mkdir = [](const char *path, mode_t mode)
	{
		switch ((*Core)->CreateDirectory(path, mode & S_IWUSR, mode & S_IXUSR))
		{
			case ActionError::OK: break;
			case ActionError::Unknown: return -EEXIST;
			default: assert(false); return -ELOOP;
		}
		return 0;
	};

	FuseCallbacks.opendir = [](const char *path, fuse_file_info *fi)
	{
		ActionResult<std::unique_ptr<ShareFile>> Result = (*Core)->OpenDirectory(path);
		switch (Result.Code)
		{
			case ActionError::OK: break;
			case ActionError::Invalid: return -ENOTDIR;
			case ActionError::Missing: return -ENOENT;
			default: assert(false); return -ELOOP;
		}
		assert(*Result);
		if (((fi->flags & O_WRONLY) || (fi->flags & O_RDWR)) && !(*Result)->CanWrite()) return -EACCES; // Can this occur?
		fi->fh = reinterpret_cast<decltype(fi->fh)>((*Result).release());
		return 0;
	};

	FuseCallbacks.releasedir = [](const char *, struct fuse_file_info *fi)
	{
		delete reinterpret_cast<ShareFile *>(fi->fh);
		return 0;
	};

	FuseCallbacks.readdir = [](const char *, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
	{
		ShareFile *File = reinterpret_cast<ShareFile *>(fi->fh);

		static unsigned int BlockCount = 100;

		while (true)
		{
			auto Files = (*Core)->GetDirectory(*File, static_cast<unsigned int>(offset), BlockCount);
			unsigned int Count = 0;
			for (auto const &File : Files)
			{
				struct stat st;
				memset(&st, 0, sizeof(st));
				ExportAttributes(File, &st);
				if (filler(buf, File.Name().c_str(), &st, offset + Count))
				{
					Count = 0;
					break;
				}
				++Count;
			}
			if (Count < BlockCount) break;
			offset += BlockCount;
		}

		return 0;
	};

	FuseCallbacks.rmdir = [](const char *path)
	{
		switch ((*Core)->Delete(path))
		{
			case ActionError::Invalid: return -ENOTDIR;
			case ActionError::Missing: return -ENOENT;
			default: assert(false); return -ELOOP;
		}
		return 0;
	};

	// File access
	/*FuseCallbacks.create = [](const char *path, mode_t mode, struct fuse_file_info *fi)
	{
		int fd = open((*Core)->TranslatePath(path).c_str(), fi->flags, mode);
		if (fd == -1) return -errno;
		fi->fh = fd;
		return 0;
	};

	FuseCallbacks.open = [](const char *path, struct fuse_file_info *fi)
	{
		ShareFile *(*Core)->Get(path).release();
		fi->fh = reinterpret_cast<decltype(fi->fh)>(ShareFile);
		int Result = open((*Core)->TranslatePath(path).c_str(), fi->flags);
		if (Result == -1) return -errno;
		close(Result);
		return 0;
	};


	FuseCallbacks.truncate = [](const char *path, off_t size)
	{
		int Result = truncate((*Core)->TranslatePath(path).c_str(), size);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.ftruncate = [](const char *, off_t size, struct fuse_file_info *fi)
	{
		int Result = ftruncate(fi->fh, size);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.unlink = [](const char *path)
	{
		int Result = unlink((*Core)->TranslatePath(path).c_str());
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.read = [](const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
	{
		int Result = pread(fi->fh, buf, size, offset);
		if (Result == -1) return -errno;
		return Result;
	};

	FuseCallbacks.write = [](const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
	{
		int Result = pwrite(fi->fh, buf, size, offset);
		if (Result == -1) return -errno;
		return Result;
	};

	FuseCallbacks.release = [](const char *, struct fuse_file_info *fi)
	{
		close(fi->fh);
		return 0;
	};

	FuseCallbacks.fsync = [](const char *, int isdatasync, struct fuse_file_info *fi)
	{
		int Result;

#ifndef HAVE_FDATASYNC
		(void) isdatasync;
		Result = fsync(fi->fh);
#else
		if (isdatasync) Result = fdatasync(fi->fh);
		else Result = fsync(fi->fh);
#endif
		if (Result == -1) return -errno;
		return 0;
	};*/

	fuse_args FuseArgs = FUSE_ARGS_INIT(0, NULL);
	fuse_opt_add_arg(&FuseArgs, argv[0]);
	fuse_opt_add_arg(&FuseArgs, argv[2]);
	fuse_opt_add_arg(&FuseArgs, "-d");

	return fuse_main(FuseArgs.argc, FuseArgs.argv, &FuseCallbacks, nullptr);
}

