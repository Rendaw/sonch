#include <string>
#include "log.h"
#include "shared.h"
#include <memory>
#include "core.h"

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

	return 0;
}

