#ifndef error_h
#define error_h

#include <string>
#include <sstream>
#include <functional>
#include <cassert>

template <int InternalID> struct ErrorBase
{
	ErrorBase(void) {}
	ErrorBase(ErrorBase<InternalID> const &Other) : Buffer(Other.Buffer.str()) {}
	template <int OtherInternalID> ErrorBase(ErrorBase<OtherInternalID> const &Other) : Buffer(Other.Buffer.str()) {}
	template <typename Whatever> ErrorBase<InternalID> &operator <<(Whatever const &Input) { Buffer << Input; return *this; }
	operator std::string(void) const { return Buffer.str(); }

	private:
		std::stringstream Buffer;
};

template <int InternalID> inline std::ostream& operator <<(std::ostream &Out, ErrorBase<InternalID> const &Error)
	{ Out << static_cast<std::string>(Error); return Out; }

#define ErrorBase ErrorBase<__COUNTER__>

typedef ErrorBase UserError;
typedef ErrorBase SystemError;

template <typename Type> inline void Assert(Type const &Value)
{
#ifndef NDEBUG
	if (!Value) throw false;
#endif
}

template <typename GotType, typename ExpectedType> inline void Assert(GotType const &Got, ExpectedType const &Expected)
{
#ifndef NDEBUG
	if (Got != Expected) throw false;
#endif
}

struct Cleanup
{
	Cleanup(std::function<void(void)> &&Procedure) : Procedure(std::move(Procedure)) {}
	~Cleanup(void) { Procedure(); }
	private:
		std::function<void(void)> Procedure;
};

#endif
