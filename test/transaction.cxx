#include "../app/transaction.h"

#include <cassert>
#include <cstdint>

int main(int argc, char **argv)
{
	try
	{
		bfs::path TransactionPath("transactiontemp");
		bfs::create_directory(TransactionPath);

		typedef ProtocolClass TransactionProtocol;
		typedef ProtocolVersionClass<TransactionProtocol> TransProtoVersion;
		
		std::mutex MainMutex;
		int Counter = 0;
		
		int a = 23;
		bool b = true;
		bool b2 = false;
		uint64_t c = 221;
		std::string d = "all good";
		struct { int whatever, whatever2; } e, e2;
		e.whatever = 88;
		e.whatever2 = 2842967;
		e2.whatever = 2929;
		e2.whatever2 = -222;

		bool Fail = false;
		
		typedef ProtocolMessageClass<TransProtoVersion, void(int A, bool B, bool B2, uint64_t C, std::string D, decltype(e) E)> Event1Type;
		typedef ProtocolMessageClass<TransProtoVersion, void(int Q, int R, int L, int M, int V)> Event2Type;

		auto Event1 = [&](int const &GotA, bool const &GotB, bool const &GotB2, uint64_t const &GotC, std::string const &GotD, decltype(e) const &GotE)
		{
			if (Fail) throw 0u;
			++Counter;
			assert(GotA == a);
			assert(GotB == b);
			assert(GotB2 == b2);
			assert(GotC == c);
			assert(GotD == d);
			assert(memcmp(&GotE, &e, sizeof(e)) == 0);
		};

		auto Event2 = [&](int const &Q, int const &R, int const &L, int const &M, int const &V)
		{
			if (Fail) throw 0u;
			assert(Q == 39);
			assert(R == -339);
			assert(L == 289200000);
			assert(M == 0);
			assert(V == 1);
		};

		{
			Transactor<Event1Type, Event2Type> Transact(MainMutex, TransactionPath, Event1, Event2);
			Transact(Type<Event1Type>(), a, b, b2, c, d, e);
			Transact(Type<Event2Type>(), 39, -339, 289200000, 0, 1);
			assert(Counter == 2);

			Counter = 0;
			Fail = true;
			try { Transact(Type<Event1Type>(), a, b, b2, c, d, e); }
			catch (unsigned int const &Error) { }
			try { Transact(Type<Event2Type>(), 39, -339, 289200000, 0, 1); }
			catch (unsigned int const &Error) { }
			assert(Counter == 0);
		}
		
		{
			Transactor<ProtocolClass> Transact(MainMutex, TransactionPath, Event1, Event2);
			assert(Counter == 2);
		}
	}
	catch (SystemError &Error) { std::cerr << Error << std::endl; return 1; }

	return 0;
}

