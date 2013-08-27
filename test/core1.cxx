#include "../app/core.h"

#include <algorithm>

int main(int, char **)
{
	bfs::path RootPath("core1root");
	Cleanup Cleanup([&]()
	{
		bfs::ifstream Log(RootPath / "log.txt");
		std::copy(std::istreambuf_iterator<char>(Log), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(std::cerr));
		std::cerr << std::flush;
		boost::filesystem::remove_all(RootPath);
	});

	try
	{
		ShareCore Core(RootPath, "core1instance1");

		// Get root, (fail) special directories, check permissions
		auto Root = Core.Get("/");
		Assert(Root);
		Assert(!Root->ID());
		Assert(!Root->Parent());
		Assert(!Root->Change());
		Assert(Root->Name() == "");
		Assert(!Root->IsFile());
		Assert(!Root->IsSplit());

		// Make sure root can't be deleted, renamed
		Assert(Core.Move(Root, "/bad"), ActionError::Illegal);
		Assert(Core.Delete(Root), ActionError::Illegal);

		// Check /splits
		auto Splits = Core.Get("/splits");
		Assert(Splits);
		Assert(!Splits->ID());
		Assert(!Splits->Parent());
		Assert(!Splits->Change());
		Assert(Splits->Name() == "splits");
		Assert(!Splits->IsFile());
		Assert(!Splits->IsSplit());

		// Make sure splits can't be deleted, renamed
		Assert(Core.Move(Splits, "/bad"), ActionError::Illegal);
		Assert(Core.Delete(Splits), ActionError::Illegal);

		// Fail instance get
		auto Instance = Core.Get("/splits/core1instance1");
		Assert(!Instance);

		// Create dir (good, bad path, not in splits, not already exists)
		bfs::path DirPath("/dir");
		auto Dir = Core.Create(DirPath, false, true, true, true, false, false, false, false, false, false);
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
	catch (...) { std::cerr << "Encountered unexpected error." << std::endl; return 1; }
	return 0;
}
