DoOnce 'app/Tupfile.lua'

ProtocolTest = Define.Executable
{
	Name = 'protocol',
	Sources = Item 'protocol.cxx',
	LinkFlags = '-lboost_system -lboost_filesystem'
}
Define.Test { Executable = ProtocolTest }

DatabaseTest = Define.Executable
{
	Name = 'database',
	Sources = Item 'database.cxx',
	LinkFlags = '-lboost_system -lboost_filesystem -lsqlite3'
}
Define.Test { Executable = DatabaseTest }

TransactionTest = Define.Executable
{
	Name = 'transaction',
	Sources = Item 'transaction.cxx',
	LinkFlags = '-lboost_system -lboost_filesystem'
}
Define.Test { Executable = TransactionTest }

Core1Test = Define.Executable
{
	Name = 'core1',
	Sources = Item 'core1.cxx',
	Objects = CoreObject,
	LinkFlags = '-lboost_system -lboost_filesystem -lsqlite3'
}
Define.Test { Executable = Core1Test }

--[[FSBasicsTest = Define.Executable
{
	Name = 'fsbasics',
	Sources = Item 'fsbasics.cxx',
	LinkFlags = '-lboost_system -lboost_filesystem'
}
Define.Test
{
	Executable = FSBasicsTest,
	Inputs = FuseApp,
	Arguments = tostring(FuseApp)
}]]

