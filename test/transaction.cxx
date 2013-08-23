#include "../app/transaction.h"

#include <cassert>
#include <cstdint>

struct E { int whatever, whatever2; };

inline size_t ProtocolGetSize(E const &Argument) { return sizeof(Argument); }
inline void ProtocolWrite(uint8_t *&Out, E const &Argument)
{
	memcpy(Out, &Argument, sizeof(Argument));
	Out += sizeof(Argument);
}
template <typename LogType> bool ProtocolRead(LogType &Log, Protocol::VersionIDType const &VersionID, Protocol::MessageIDType const &MessageID, Protocol::BufferType const &Buffer, Protocol::SizeType &Offset, E &Data)
{
	if (Buffer.size() < StrictCast(Offset, size_t) + sizeof(Data))
	{
		Log.Debug() << "End of file reached prematurely reading message body E size " << sizeof(Data) << ", message length doesn't match contents (version " << *VersionID << ", type " << *MessageID << ")";
		assert(false);
		return false;
	}

	Data = *reinterpret_cast<E const *>(&Buffer[*Offset]);
	Offset += (Protocol::SizeType::Type)sizeof(E);
	return true;
}

DefineProtocol(TransactionProtocol);
DefineProtocolVersion(TransProtoVersion, TransactionProtocol);
DefineProtocolMessage(Event1Type, TransProtoVersion, void(int a, bool b, bool b2, uint64_t c, std::string d, E e));
DefineProtocolMessage(Event2Type, TransProtoVersion, void(int q, int r, int l, int m, int v));

int main(int argc, char **argv)
{
	try
	{
		bfs::path TransactionPath("transactiontemp");
		bfs::create_directory(TransactionPath);
		Cleanup Cleanup([&TransactionPath]() { try { bfs::remove_all(TransactionPath); } catch (...) {} });

		std::mutex MainMutex;
		int Counter = 0;

		int a = 23;
		bool b = true;
		bool b2 = false;
		uint64_t c = 221;
		std::string d = "all good";
		E e, e2;
		e.whatever = 88;
		e.whatever2 = 2842967;
		e2.whatever = 2929;
		e2.whatever2 = -222;

		bool Fail = false;

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
			++Counter;
			assert(Q == 39);
			assert(R == -339);
			assert(L == 289200000);
			assert(M == 0);
			assert(V == 1);
		};

		{
			Transactor<Event1Type, Event2Type> Transact(MainMutex, TransactionPath, Event1, Event2);
			Transact(Event1Type(), a, b, b2, c, d, e);
			Transact(Event2Type(), 39, -339, 289200000, 0, 1);
			assert(Counter == 2);

			Counter = 0;
			Fail = true;
			try { Transact(Event1Type(), a, b, b2, c, d, e); }
			catch (unsigned int const &Error) { }
			assert(Counter == 0);
		}

		{
			Fail = false;
			Transactor<Event1Type, Event2Type> Transact(MainMutex, TransactionPath, Event1, Event2);
			assert(Counter == 1);
		}
	}
	catch (SystemError &Error) { std::cerr << Error << std::endl; return 1; }
	catch (...) { std::cerr << "Unknown error" << std::endl; throw; }

	return 0;
}

