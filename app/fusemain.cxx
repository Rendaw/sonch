#include "log.h"
#include "shared.h"
#include "protocol.h"

#include <string>
#include <memory>
#include <cassert>
#include <dirent.h>
#include <sys/time.h>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#define _FILE_OFFSET_BITS 64
#define _REENTRANT
#define FUSE_USE_VERSION 26
#include <fuse.h>

#define App "sonch"
#define SplitDir "splits"

typedef uint64_t UUID;

struct
{
	std::string InstanceName;
	UUID InstanceID;
	std::string InstanceFilename;

	boost::filesystem::path RootPath;
} PreinitContext;

class FuseContext
{
	public:
		FuseContext() : 
			InstanceRoot((PreinitContext.RootPath / "." App / SplitDir).string()), 
			MainInstanceRoot((PreinitContext.RootPath / "." App / SplitDir / PreinitContext.InstanceName).string()), 
			SplitPath(SplitDir), 
			Log((PreinitContext.RootPath / "log.txt").string())
		{
		}
		
		// Path methods
		std::string TranslatePath(std::string const &Path)
		{
			std::string Out;
			if (IsSplitPath(Path)) Out = InstanceRoot + Path;
			else Out = MainInstanceRoot + Path;
			Log.Debug() << "Translating " << Path << " to " << Out;
			return Out;
		}

		bool IsSplitPath(std::string const &Path)
		{
			assert(Path.length() >= 1);
			return (Path.length() >= SplitPath.length() - 1) &&
					(((Path.length() == SplitPath.length() - 1) && 
						(Path.compare(0, SplitPath.length() - 1, SplitPath) == 0)) ||
					(Path.compare(0, SplitPath.length(), SplitPath) == 0));
		}

	private:
		std::string const InstanceRoot;
		std::string const MainInstanceRoot;
		std::string const SplitPath;
	public:
		FileLog Log;
};

std::unique_ptr<FuseContext> Context;

static bool ValidateFilename(std::string const &Filename)
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
}

int main(int argc, char **argv)
{
	StandardOutLog Log("initialization");

	if (argc <= 2)
	{
		Log.Note() << ("Usage: " App " LOCATION MOUNTPOINT [NAME]\n"
			"\tMounts " App " share LOCATION at MOUNTPOINT.  If LOCATION does not exist, creates a new share with NAME.");
		return 0;
	}

	std::string InstanceName;
	UUID InstanceID;
	std::string InstanceFilename;

	boost::filesystem::path RootPath;

	try
	{
		std::string InstanceName;
		if (argc >= 3) InstanceName = argv[3];

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
			RootPath = argv[1];

			typedef ProtocolClass StaticDataProtocol;
			typedef ProtocolMessageClass<ProtocolVersionClass<StaticDataProtocol>, 
				void(std::string InstanceName, UUID InstanceUUID)> StaticDataV1;

			if (!boost::filesystem::exists(RootPath))
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
				boost::filesystem::create_directory(RootPath);
				boost::filesystem::create_directory(RootPath / "." App);
				boost::filesystem::create_directory(RootPath / "." App / SplitDir);
				boost::filesystem::create_directory(RootPath / "." App / SplitDir / InstanceFilename);

				{
					boost::filesystem::path StaticDataPath = RootPath / "." App / "static";
					boost::filesystem::ofstream Out(StaticDataPath, std::ofstream::out | std::ofstream::binary);
					auto const &Data = StaticDataV1::Write(InstanceName, InstanceID);
					if (!Out) throw SystemError() << "Could not create file '" << StaticDataPath << "'.";
					Out.write((char const *)&Data[0], Data.size());
				}

				{
					boost::filesystem::path ReadmePath = RootPath / App "-share-readme.txt";
					boost::filesystem::ofstream Out(ReadmePath);
					if (!Out) throw SystemError() << "Could not create file '" << ReadmePath << "'.";
					Out << "Do not modify the contents of this directory.\n\nThis directory is the unmounted data for a " App " share.  Modifying the contents could cause data corruption.  Moving and changing the permissions for this folder only (and not it's contents) is fine." << std::endl;
				}
			}
			else
			{
				// Share exists, restore state
				// --
				if (!InstanceName.empty())
					Log.Warn() << "Share exists, ignoring all other arguments.";

				// Load static data
				boost::filesystem::ifstream In(RootPath / "." App / "static", std::ifstream::in | std::ifstream::binary);
				Protocol::Reader<StandardOutLog> Reader(Log);
				Reader.Add<StaticDataV1>([&](std::string &ReadInstanceName, UUID &ReadInstanceUUID) 
				{
					InstanceName = ReadInstanceName;
					InstanceID = ReadInstanceUUID;
				});
				bool Success = Reader.Read(In);
				if (!Success)
					throw SystemError() << "Could not read static data, file may be corrupt.";
				
				InstanceFilename = GetInstanceFilename(InstanceName, InstanceID);
			}
		}
		catch (boost::filesystem::filesystem_error &Error)
			{ throw SystemError() << Error.what(); }
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
	fuse_operations FuseCallbacks{0};

	// Non-filesystem events
	PreinitContext.InstanceName = InstanceName;
	PreinitContext.InstanceID = InstanceID;
	PreinitContext.InstanceFilename = InstanceFilename;
	PreinitContext.RootPath = RootPath;
	FuseCallbacks.init = [](fuse_conn_info *conn) -> void *
	{
		Context.reset(new FuseContext());
		return nullptr;
	};

	// Read actions
	// Write actions
	// TODO: Categorize
	FuseCallbacks.getattr = [](const char *path, struct stat *stbuf)
	{
		int Result = lstat(Context->TranslatePath(path).c_str(), stbuf);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.fgetattr = [](const char *, struct stat *stbuf, struct fuse_file_info *fi)
	{
		int Result = fstat(fi->fh, stbuf);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.access = [](const char *path, int mask)
	{
		int Result = access(Context->TranslatePath(path).c_str(), mask);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.readlink = [](const char *path, char *buf, size_t size)
	{
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
		int Result = mkdir(Context->TranslatePath(path).c_str(), mode);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.unlink = [](const char *path)
	{
		int Result = unlink(Context->TranslatePath(path).c_str());
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.rmdir = [](const char *path)
	{
		int Result = rmdir(Context->TranslatePath(path).c_str());
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.symlink = [](const char *from, const char *to)
		{ return -EPERM; };

	FuseCallbacks.rename = [](const char *from, const char *to)
	{
		int Result = rename(Context->TranslatePath(from).c_str(), Context->TranslatePath(to).c_str());
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.link = [](const char *from, const char *to)
		{ return -EPERM; };

	FuseCallbacks.chmod = [](const char *path, mode_t mode)
	{
		int Result = chmod(Context->TranslatePath(path).c_str(), mode);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.chown = [](const char *path, uid_t uid, gid_t gid)
	{
		int Result = lchown(Context->TranslatePath(path).c_str(), uid, gid);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.truncate = [](const char *path, off_t size)
	{
		int Result = truncate(Context->TranslatePath(path).c_str(), size);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.ftruncate = [](const char *, off_t size, struct fuse_file_info *fi)
	{
		int Result = ftruncate(fi->fh, size);
		if (Result == -1) return -errno;
		return 0;
	};

	FuseCallbacks.utimens = [](const char *path, const struct timespec ts[2])
	{
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
		int fd = open(Context->TranslatePath(path).c_str(), fi->flags, mode);
		if (fd == -1) return -errno;
		fi->fh = fd;
		return 0;
	};

	FuseCallbacks.open = [](const char *path, struct fuse_file_info *fi)
	{
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
		int Result = statvfs(Context->TranslatePath(path).c_str(), stbuf);
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

	fuse_args FuseArgs = FUSE_ARGS_INIT(0, NULL);
	fuse_opt_add_arg(&FuseArgs, argv[0]);
	fuse_opt_add_arg(&FuseArgs, argv[2]);
	fuse_opt_add_arg(&FuseArgs, "-d");
     
	return fuse_main(FuseArgs.argc, FuseArgs.argv, &FuseCallbacks, nullptr);
}

