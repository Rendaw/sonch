CoreObject = Define.Object{
	Source = Item 'core.cxx'
}

FuseApp = Define.Executable{
	Name = 'sonch',
	Sources = Item 'fusemain.cxx',
	Objects = CoreObject,
	LinkFlags = '-lfuse -lboost_system -lboost_filesystem -lsqlite3'
}
