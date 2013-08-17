#include "../app/core.h"

int main(int, char)
{
	bfs::path Root("./core1root");
	Cleanup Cleanup([&]() { boost::filesystem::remove_all(Root); });

	try
	{
		ShareCore Core(Root, "core1instance1");
	}
	catch (SystemError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (UserError const &Error) { std::cerr << Error << std::endl; return 1; }
	catch (...) { std::cerr << "Encountered unexpected error." << std::endl; return 1; }
	return 0;
}
