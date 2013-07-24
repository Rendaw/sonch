#ifndef shared_h
#define shared_h

#define App "sonch"
typedef uint64_t UUID;

struct StringStream : std::stringstream
{
	using std::stringstream::stringstream;
	operator std::string(void) { return str(); }
};

#endif

