#include "log.h"
#include "shared.h"
#include "core.h"

#include <string>
#include <memory>
#include <cassert>

#define _FILE_OFFSET_BITS 64
#define _REENTRANT
#define FUSE_USE_VERSION 26
#include <fuse.h>

int main(int argc, char **argv)
{
	StandardOutLog Log("Initialization");

	if (argc <= 1)
	{
		Log.Note() << ("Usage: " App " LOCATION [NAME]\n"
			"\tMounts " App " share LOCATION.  If LOCATION does not exist, creates a new share with NAME.");
		return 0;
	}

	std::string PhysicalRoot = argv[1];
	std::string InstanceName;
	if (argc >= 2) InstanceName = argv[2];

	std::unique_ptr<Core> AppCore;
	try
	{
		AppCore.reset(new Core(PhysicalRoot, InstanceName));
	}
	catch (UserError &Message)
	{
		Log.Error() << Message;
		return 1;
	}
	catch (SystemError &Message)
	{
		Log.Error() << "Encountered a system error during initialization.\n\t" << Message;
		return 1;
	}

	// Set up fuse, start fuse
	// --
	
	class FuseContext
	{
		public:
			FuseContext(Core *AppCore) : 
				AppCore(AppCore),
				InstanceRoot(AppCore->GetInstancePath()), MainInstanceRoot(AppCore->GetMainInstancePath()), 
				SplitPath(AppCore->GetMountSplitPath())
			{
			}
			
			// Path methods
			std::string TranslatePath(std::string const &Path) const
			{
				if (IsSplitPath(Path)) return InstanceRoot + Path;
				return MainInstanceRoot + Path;
			}

			bool IsSplitPath(std::string const &Path) const
			{
				assert(Path.length() >= 1);
				return (Path.length() >= SplitPath.length() - 1) &&
						(((Path.length() == SplitPath.length() - 1) && 
							(Path.compare(0, SplitPath.length() - 1, SplitPath) == 0)) ||
						(Path.compare(0, SplitPath.length(), SplitPath) == 0));
			}

		private:
			Core *AppCore;
			std::string const InstanceRoot;
			std::string const MainInstanceRoot;
			std::string const SplitPath;
	} Context(AppCore.get());
	
	fuse_operations FuseCallbacks{0};

	// Non-filesystem events
	FuseCallbacks.init = [](fuse_conn_info *conn)
	{
		Core *AppCore = conn->user_data;
		//AppCore->StartThreads();
		return AppCore;
	};

	// Read actions
	// Write actions
	// TODO: Categorize
	FuseCallbacks.getattr = [](const char *path, struct stat *stbuf)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = lstat(Context->TranslatePath(path).c_str(), stbuf);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.fgetattr = [](const char *, struct stat *stbuf, struct fuse_file_info *fi)
	{
		int Result = fstat(fi->fh, stbuf);
		if (Result == -1) return -errno;
		return 0;
	}

	FuseCallbacks.access = [](const char *path, int mask)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = access(Context->TranslatePath(path).c_str(), mask);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.readlink = [](const char *path, char *buf, size_t size)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = readlink(Context->TranslatePath(path).c_str(), buf, size - 1);
		if (Result == -1) return -errno;
		buf[Result] = '\0';
		return 0;
	};

	FuseCallbacks.readdir = [](const char *, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
	{
		DIR *dp = reinterpret_cast<DIR *>(fi->fh);
	
		seekdir(dp, offset);
		
		struct dirent *de;
		while ((de = readdir(dp)) != NULL) 
		{
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if (filler(buf, de->d_name, &st, telldir(dp)))
				break;
		}

		return 0;
	};

	FuseCallbacks.releasedir = [](const char *, struct fuse_file_info *fi)
	{
		DIR *dp = reinterpret_cast<DIR *>(fi->fh);
		closedir(dp);
		return 0;
	};

	FuseCallbacks.mknod = [](const char *, mode_t mode, dev_t)
		{ return -EPERM; };

	FuseCallbacks.mkdir = [](const char *path, mode_t mode)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = mkdir(Context->TranslatePath(path).c_str(), mode);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.unlink = [](const char *path)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = unlink(Context->TranslatePath(path));
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.rmdir = [](const char *path)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = rmdir(Context->TranslatePath(path));
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.symlink = [](const char *from, const char *to)
		{ return -EPERM; };

	FuseCallbacks.rename = [](const char *from, const char *to)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = rename(Context->TranslatePath(from), Context->TranslatePath(to));
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.link = [](const char *from, const char *to)
		{ return -EPERM; };

	FuseCallbacks.chmod = [](const char *path, mode_t mode)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = chmod(Context->TranslatePath(path).c_str(), mode);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.chown = [](const char *path, uid_t uid, gid_t gid)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = lchown(Context->TranslatePath(path).c_str(), uid, gid);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.truncate = [](const char *path, off_t size)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = truncate(Context->TranslatePath(path).c_str(), size);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.ftruncate = [](const char *, off_t size, struct fuse_file_info *fi)
	{
		Result = ftruncate(fi->fh, size);
		if (Result == -1) return -errno;
		return 0;
	}

	FuseCallbacks.utimens = [](const char *path, const struct timespec ts[2])
	{
		FuseContext *Context = get_fuse_context().private_data;
		struct timeval tv[2];
		tv[0].tv_sec = ts[0].tv_sec;
		tv[0].tv_usec = ts[0].tv_nsec / 1000;
		tv[1].tv_sec = ts[1].tv_sec;
		tv[1].tv_usec = ts[1].tv_nsec / 1000;
		int Result = utimes(Context->TranslatePath(path).c_str(), tv);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.create = [](const char *path, mode_t mode, struct fuse_file_info *fi)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int fd = open(Context->TranslatePath(path).c_str(), fi->flags, mode);
		if (fd == -1) return -errno;
		fi->fh = fd;
		return 0;
	}

	FuseCallbacks.open = [](const char *path, struct fuse_file_info *fi)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = open(Context->TranslatePath(path).c_str(), fi->flags);
		if (Result == -1) return -errno;
		close(Result);
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

	FuseCallbacks.statfs = [](const char *path, struct statvfs *stbuf)
	{
		FuseContext *Context = get_fuse_context().private_data;
		int Result = statvfs(Context->TranslatePath(path), stbuf);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.release = [](const char *, struct fuse_file_info *fi)
	{
		close(fi->fh);
		return 0;
	};

	FuseCallbacks.fsync = [](const char *path, int isdatasync, struct fuse_file_info *fi)
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
	};

	FuseCallbacks.setxattr = [](const char *path, const char *name, const char *value, size_t size, int flags)
		{ return -EPERM; };

	FuseCallbacks.getxattr = [](const char *path, const char *name, char *value, size_t size)
		{ return -EPERM; };

	FuseCallbacks.listxattr = [](const char *path, char *list, size_t size)
		{ return -EPERM; };

	FuseCallbacks.removexattr = [](const char *path, const char *name)
		{ return -EPERM; };
    
	fuse_stat = fuse_main(argc, argv, &FuseCallbacks, Context);

	return 0;
}

