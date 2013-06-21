#ifndef log_h
#define log_h

#include <sstream>
#include <iostream>

struct NullLogStream
{
	template <typename InputType> NullLogStream &operator <<(InputType const &) { return *this; }
};

struct NullLog
{
	NullLogStream Debug(void) { return NullLogStream(); }
	NullLogStream Note(void) { return NullLogStream(); }
	NullLogStream Warn(void) { return NullLogStream(); }
	NullLogStream Error(void) { return NullLogStream(); }
};

template <bool Important> struct StandardLogStream : public std::stringstream
{
	StandardLogStream(std::ostream &Output, std::string const &Prefix) : Output(Output) { *this << Prefix; }
	~StandardLogStream() { *this << '\n'; if (Important) Output << str() << std::flush; else Output << str(); }
	std::ostream &Output;
};

struct StandardOutLog
{
	StandardOutLog(std::string const &Prefix)
	{
#ifndef NDEBUG
		std::stringstream DebugPrefixStream;
		DebugPrefixStream << "Debug (" << Prefix << "): ";
		DebugPrefix = DebugPrefixStream.str();
#endif
		std::stringstream NotePrefixStream;
		NotePrefixStream << "Note (" << Prefix << "): ";
		NotePrefix = NotePrefixStream.str();
		std::stringstream WarnPrefixStream;
		WarnPrefixStream << "Warn (" << Prefix << "): ";
		WarnPrefix = WarnPrefixStream.str();
		std::stringstream ErrorPrefixStream;
		ErrorPrefixStream << "Error (" << Prefix << "): ";
		ErrorPrefix = ErrorPrefixStream.str();
	}
#ifndef NDEBUG
	StandardLogStream<false> Debug(void) { return {std::cout, DebugPrefix}; }
#else
	NullLogStream Debug(void) { return NullLogStream(); }
#endif
	StandardLogStream<false> Note(void) { return {std::cout, NotePrefix}; }
	StandardLogStream<false> Warn(void) { return {std::cout, WarnPrefix}; }
	StandardLogStream<true> Error(void) { return {std::cerr, ErrorPrefix}; }

	private:
		std::string 
#ifndef NDEBUG
			DebugPrefix,
#endif
			NotePrefix, WarnPrefix, ErrorPrefix;
};

#endif

