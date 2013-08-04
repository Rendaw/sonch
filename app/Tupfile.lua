DatabaseObject = Define.Object{
	Source = Item 'database.cxx'
}

FuseApp = Define.Executable{
	Name = 'sonch', 
	Sources = Item 'fusemain.cxx',
	Objects = DatabaseObject,
	LinkFlags = '-lfuse -lboost_system -lboost_filesystem -lsqlite3'
}

