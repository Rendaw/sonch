FuseApp = Define.Executable{
	Name = 'sonch', 
	Sources = Item 'fusemain.cxx',
	LinkFlags = '-lfuse -lboost_system -lboost_filesystem'
}

