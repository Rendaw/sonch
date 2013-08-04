DoOnce 'app/Tupfile.lua'

ProtocolTest = Define.Executable
{
	Name = 'protocol', 
	Sources = Item 'protocol.cxx'
}
Define.Test { Executable = ProtocolTest }

DatabaseTest = Database.Executable
{
	Name = 'database',
	Sources = Item 'database.cxx',
	Objects = DatabaseObject,
	LinkFlags = '-lsqlite3'
}
Define.Test { Executable = DatabaseTest }

FSBasicsTest = Define.Executable
{
	Name = 'fsbasics', 
	Sources = Item 'fsbasics.cxx',
	LinkFlags = '-lboost_system -lboost_filesystem'
}
--[[Define.Test 
{ 
	Executable = FSBasicsTest,
	Inputs = FuseApp,
	Arguments = tostring(FuseApp)
}]]

