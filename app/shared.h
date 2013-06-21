#ifndef shared_h
#define shared_h

#define App "sonch"
#define SplitDir "splits"
#define Version 0

class ErrorBase
{
	public:
		ErrorBase(void) {}
		ErrorBase(ErrorBase const &Other) : Buffer(Other.Buffer.str()) {}
		template <typename Whatever> ErrorBase &operator <<(Whatever const &Input) { Buffer << Input; return *this; }
		operator std::string(void) const { return Buffer.str(); }
	private:
		std::stringstream Buffer;
};

inline std::ostream& operator <<(std::ostream &Out, ErrorBase const &Error)
	{ Out << static_cast<std::string>(Error); return Out; }

struct UserError : ErrorBase {};
struct SystemError : ErrorBase {};

#endif

