#ifndef core_h
#define core_h

#include <string>
#include <boost/filesystem.hpp>

class Core
{
	public:
		Core(std::string const &Root, std::string InstanceName = std::string());

		std::string GetInstancePath(void) const;
		std::string GetMainInstancePath(void) const;
		std::string GetMountSplitPath(void) const;
	private:
		boost::filesystem::path Root, MountRoot;
};

#endif

