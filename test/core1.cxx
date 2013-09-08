#include "../app/core.h"

#include <algorithm>

int main(int, char **)
{
	try
	{
		bfs::path ExternalRootPath("core1root");
		Cleanup Cleanup([&]()
		{
			bfs::ifstream Log(ExternalRootPath / "log.txt");
			std::copy(std::istreambuf_iterator<char>(Log), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(std::cerr));
			std::cerr << std::flush;
			boost::filesystem::remove_all(ExternalRootPath);
		});

		ShareCore Core(ExternalRootPath, "core1instance1");

		// Get root, (fail) special directories, check permissions
		bfs::path const RootPath = "/";
		auto Root = Core.Get("/");
		Assert(Root);
		Assert(!Root->ID());
		Assert(!Root->Parent());
		Assert(!Root->Change());
		Assert(Root->Name() == "");
		Assert(!Root->IsFile());
		Assert(!Root->IsSplit());

		// Make sure root can't be deleted, renamed
		Assert(Core.Move(RootPath, "/bad"), ActionError::Illegal);
		Assert(Core.Delete(RootPath), ActionError::Illegal);

		// Check /splits
		auto const SplitsPath = RootPath / "splits";
		auto Splits = Core.Get(SplitsPath);
		Assert(Splits);
		Assert(!Splits->ID());
		Assert(!Splits->Parent());
		Assert(!Splits->Change());
		Assert(Splits->Name() == "splits");
		Assert(!Splits->IsFile());
		Assert(!Splits->IsSplit());

		// Make sure splits can't be deleted, renamed
		Assert(Core.Move(SplitsPath, RootPath / "bad"), ActionError::Illegal);
		Assert(Core.Delete(SplitsPath), ActionError::Illegal);

		// Fail instance get
		auto Instance = Core.Get(RootPath / "splits" / "core1instance1");
		Assert(!Instance);

		// Create dir (good, bad path, not in splits, not already exists)
		bfs::path DirPath(RootPath / "dir");
		auto DirResult = Core.CreateDirectory(DirPath, true, true);
		assert(DirResult == ActionError::OK);
		auto Dir = Core.Get(DirPath);
		Assert(Dir);
		Assert(Dir->ID().Instance, Counter::Type(0));
		Assert(Dir->ID().Index, UUID::Type(1));
		Assert(Dir->Change().Instance, Counter::Type(0));
		Assert(Dir->Change().Index, UUID::Type(0));
		Assert(Dir->Parent().Instance, Root->ID().Instance);
		Assert(Dir->Parent().Index, Root->ID().Index);

		// Rename dir (good, bad path, not in splits, not already exists)
		// Rename dir again, check ancestry (1)
		// Create subdir
		// Delete subdir
		// Create dir
		// Move to subdir
		// List subdirs
	}
	catch (SystemError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (UserError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (...) { std::cerr << "Encountered unexpected error." << std::endl; throw; }
	return 0;
}
