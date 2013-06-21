#include "shared.h"

std::ostream& operator <<(std::ostream &Out, ErrorBase const &Error)
	{ Out << static_cast<std::string>(Error); return Out; }

