#ifndef shared_h
#define shared_h

struct String
{
	String(void) {}
	template <typename Whatever> String &operator <<(Whatever const &Input) { Buffer << Input; return *this; }
	operator std::string(void) const { return Buffer.str(); }

	private:
		std::stringstream Buffer;
};

#endif

