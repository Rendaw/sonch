#include <functional>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

struct AssertError
{
	int Line;
	std::string File;
};

void Assert_(int Line, char const *File, bool Condition)
	{ if (!Condition) throw AssertError{Line, File}; }

#define Assert(Condition) Assert_(__LINE__, __FILE__, Condition)

int main(int argc, char **argv)
{ try {
	boost::filesystem::path Root("testshare_base");
	boost::filesystem::path Mount("testshare");

	struct Cleanup
	{
		Cleanup(std::function<void(void)> &&Procedure) : Procedure(std::move(Procedure)) {}
		~Cleanup(void) { Procedure(); }
		private:
			std::function<void(void)> Procedure;
	} Cleanup([&]()
	{
		system(("fusermount -u " + Mount.string()).c_str());

		boost::filesystem::remove_all(Root);
		boost::filesystem::remove(Mount);
	});

	Assert(argc >= 2);

	boost::filesystem::create_directory(Mount);
	Assert(system((std::string(argv[1]) + " " + Root.string() + " " + Mount.string() + " testshare").c_str()) == 0);
	boost::filesystem::path Instance = [&]() {
		boost::filesystem::directory_iterator End;
		for (boost::filesystem::directory_iterator File(Root / ".sonch" / "splits"); File != End; ++File)
			return boost::filesystem::path(*File);
		return boost::filesystem::path();
	}();
	Assert(!Instance.empty());
	std::cout << "Main instance path is " << Instance.string() << std::endl;
	
	// Exepected successes
	// --
	// mkdir
	boost::filesystem::path Directory = Mount / "directory1";
	Assert(boost::filesystem::create_directory(Directory));
	Assert(boost::filesystem::exists(Directory));

	// Create file (write), write, close
	boost::filesystem::path File = Directory / "file1";
	{
		boost::filesystem::ofstream FileStream(File);
		FileStream << "test 12345";
	}
	Assert(boost::filesystem::exists(File));

	// Open read, read, close
	{
		boost::filesystem::ifstream FileStream(File);
		std::string Line;
		Assert(std::getline(FileStream, Line));
		Assert(Line == "test 12345");
	}

	// chmod, chgrp file
	Assert(system(("chmod 000 " + File.string()).c_str()) == 0);
	Assert(system(("chmod 777 " + File.string()).c_str()) == 0);
	Assert(system(("chown `id -nu` " + File.string()).c_str()) != 0);
	Assert(system(("chgrp `id -ng` " + File.string()).c_str()) != 0);

	// Delete
	Assert(boost::filesystem::remove(File));
	Assert(!boost::filesystem::exists(File));

	// chmod, chgrp dir
	Assert(system(("chmod 000 " + Directory.string()).c_str()) == 0);
	Assert(system(("chmod 777 " + Directory.string()).c_str()) == 0);
	Assert(system(("chown `id -nu` " + Directory.string()).c_str()) != 0);
	Assert(system(("chgrp `id -ng` " + Directory.string()).c_str()) != 0);
	
	// rmdir
	Assert(boost::filesystem::remove(Directory));
	Assert(!boost::filesystem::exists(Directory));

	// Expected failures
	// --
	// Open non-existant file
	{
		boost::filesystem::path Path = Mount / "fail1";
		boost::filesystem::ifstream File(Path);
		Assert(!File);
		Assert(!boost::filesystem::exists(Path));
	}

	// Delete non-existant file
	{
		boost::filesystem::path Path = Mount / "fail2";
		Assert(!boost::filesystem::remove(Path));
	}

	// Link
	{
		boost::filesystem::path From = Mount / "fail3";
		{
			boost::filesystem::ofstream File(From);
			Assert(File);
		}
		boost::filesystem::path To = Mount / "fail4";
		try
		{
			boost::filesystem::create_symlink(From, To);
			Assert(false);
		}
		catch (...) {}
		Assert(!boost::filesystem::exists(To));
		Assert(boost::filesystem::remove(From));
	}

	Assert(system(("fusermount -u " + Mount.string()).c_str()) == 0);

	// Re-mount
	Assert(system((std::string(argv[1]) + " " + Root.string() + " " + Mount.string()).c_str()) == 0);
} catch (...) { std::cout << "Hoi" << std::endl; throw; } return 0; }
