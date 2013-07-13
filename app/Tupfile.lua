FuseApp = Executable{
	Name = 'sonch', 
	Sources = {Item{'fusemain.cxx'}, Item{'core.cxx'}},
	LinkFlags = ' -lfuse'
}

