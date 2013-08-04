#include "database.h"

Database::Database(bfs::path const &Path) : Context(nullptr)
{
	if ((Path.empty() && (sqlite3_open(":memory:", &Context) != 0)) ||
		(!Path.empty() && (sqlite3_open(Path.string().c_str(), &Context) != 0)))
		throw SystemError() << "Could not create database: " << sqlite3_errmsg(Context);
}

Database::~Database(void)
{
	sqlite3_close(Context);
}

