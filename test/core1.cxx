#include "../app/core.h"

#include <algorithm>

int main(int, char **)
{
	try
	{
		bfs::path ExternalRootPath("core1root");
		//boost::filesystem::remove_all(ExternalRootPath); // Cleanup pre
		Cleanup Cleanup([&]() // Cleanup post
		{
			bfs::ifstream Log(ExternalRootPath / "log.txt");
			std::copy(std::istreambuf_iterator<char>(Log), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(std::cerr));
			std::cerr << std::flush;
			boost::filesystem::remove_all(ExternalRootPath);
		});

		ShareCore Core(ExternalRootPath, "core1instance1");

		/// Base stuff, get
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

		/// Create dir tests
		// Create splits
		Assert(Core.CreateDirectory(RootPath / "splits" / "dir", true, true), ActionError::Illegal);

		// Create splits 2
		Assert(Core.CreateDirectory(RootPath / "splits", true, true), ActionError::Illegal);

		// Create dir
		bfs::path DirPath(RootPath / "dir");
		Assert(Core.CreateDirectory(DirPath, true, true), ActionError::OK);
		auto Dir = Core.Get(DirPath);
		Assert(Dir);
		Assert(Dir->ID().Instance, Counter::Type(0));
		Assert(Dir->ID().Index, UUID::Type(1));
		Assert(Dir->Change().Instance, Counter::Type(0));
		Assert(Dir->Change().Index, UUID::Type(0));
		Assert(Dir->Parent().Instance, Root->ID().Instance);
		Assert(Dir->Parent().Index, Root->ID().Index);

		// Create already exists
		Assert(Core.CreateDirectory(RootPath / "dir", true, true), ActionError::Exists);

		// Create subdir
		bfs::path SubdirPath(DirPath / "subdir");
		Assert(Core.CreateDirectory(SubdirPath, true, true), ActionError::OK);
		auto Subdir = Core.Get(SubdirPath);
		Assert(Subdir.Code, ActionError::OK);
		Assert(Subdir->ID().Instance, Counter::Type(0));
		Assert(Subdir->ID().Index, UUID::Type(2));
		Assert(Subdir->Change().Instance, Counter::Type(0));
		Assert(Subdir->Change().Index, UUID::Type(0));
		Assert(Subdir->Parent().Instance, Dir->ID().Instance);
		Assert(Subdir->Parent().Index, Dir->ID().Index);

		/// Delete tests
		// Delete subdir
		Assert(Core.Delete(SubdirPath), ActionError::OK);

		/// Rename tests
		// Move into splits
		Assert(Core.Move(DirPath, SplitsPath), ActionError::Illegal);

		// Rename dir
		Assert(Core.Move(DirPath, RootPath / "dir1b"), ActionError::OK);
		Assert(Core.Move(RootPath / "dir1b", DirPath), ActionError::OK);
		Dir = Core.Get(DirPath);
		Assert(Dir);
		Assert(Dir->ID().Instance, Counter::Type(0));
		Assert(Dir->ID().Index, UUID::Type(1));
		Assert(Dir->Change().Instance, Counter::Type(0));
		Assert(Dir->Change().Index, UUID::Type(2));
		Assert(Dir->Parent().Instance, Root->ID().Instance);
		Assert(Dir->Parent().Index, Root->ID().Index);

		// Move to subdir
		bfs::path Subdir2Path(RootPath / "subdir");
		Assert(Core.CreateDirectory(Subdir2Path, true, true), ActionError::OK);
		auto Subdir2 = Core.Get(SubdirPath);
		Assert(Subdir2);
		Assert(Subdir2->ID().Instance, Counter::Type(0));
		Assert(Subdir2->ID().Index, UUID::Type(3));
		Assert(Subdir2->Change().Instance, Counter::Type(0));
		Assert(Subdir2->Change().Index, UUID::Type(0));
		Assert(Subdir2->Parent().Instance, Root->ID().Instance);
		Assert(Subdir2->Parent().Index, Root->ID().Index);
		Assert(Core.Move(Subdir2Path, DirPath / "subdir"), ActionError::OK);

		/// Dir list tests
		// Bad path
		Assert(Core.OpenDirectory(RootPath / "missing").Code, ActionError::Missing);

		// Splits
		auto SplitListHandle = Core.OpenDirectory(SplitsPath);
		Assert(SplitListHandle);
		Assert(*SplitListHandle);
		auto SplitList = Core.GetDirectory(**SplitListHandle, 0, 100);
		Assert(SplitList.size(), 0u);

		// 0 subdirs
		auto Subdir2ListHandle = Core.OpenDirectory(Subdir2Path);
		Assert(Subdir2ListHandle);
		Assert(*Subdir2ListHandle);
		auto Subdir2List = Core.GetDirectory(**Subdir2ListHandle, 0, 100);
		Assert(Subdir2List.size(), 0u);

		// 1 subdir
		auto OpenDir = Core.OpenDirectory(DirPath);
		Assert(OpenDir);
		Assert(*OpenDir);
		auto Children = Core.GetDirectory(**OpenDir, 0, 100);
		Assert(Children.size(), 1u);
		Assert(Children[0].ID().Instance, Subdir2->ID().Instance);
		Assert(Children[0].ID().Index, Subdir2->ID().Index);
		Assert(Children[0].Change().Instance, Subdir2->Change().Instance);
		Assert(Children[0].Change().Index, Subdir2->Change().Index);
		Assert(Children[0].Parent().Instance, Subdir2->Parent().Instance);
		Assert(Children[0].Parent().Index, Subdir2->Parent().Index);
	}
	catch (SystemError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (UserError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (...) { std::cerr << "Encountered unexpected error." << std::endl; throw; }
	return 0;
}
