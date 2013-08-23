#include <cstdint>
#include <iomanip>
#include <limits>
#include "../app/protocol.h"
#include "../app/log.h"

void AssertEquals_(std::vector<uint8_t> &Got, std::vector<uint8_t> const &Expected, int Line, char const *File)
{
	std::cout << "Test " << File << ":" << Line;
	for (unsigned int Start = 0; Start < std::max(Got.size(), Expected.size()); Start += 8)
	{
		std::cout << "\n     Got " << std::dec << Start << ":";
		for (unsigned int Offset = Start; Offset < std::min((long unsigned int)Start + 8, std::max(Got.size(), Expected.size())); ++Offset)
		{
			if (Offset >= Got.size()) break;
			std::cout << " " << std::setw(2) << std::hex << (unsigned int)Got[Offset];
		}
		std::cout << "\nExpected " << std::dec << Start << ":";
		for (unsigned int Offset = Start; Offset < std::min((long unsigned int)Start + 8, std::max(Got.size(), Expected.size())); ++Offset)
		{
			if (Offset >= Expected.size()) break;
			std::cout << " " << std::setw(2) << std::hex << (unsigned int)Expected[Offset];
		}
	}

	std::cout << std::dec << std::endl;

	for (unsigned int Offset = 0; Offset < std::max(Got.size(), Expected.size()); ++Offset)
	{
		assert(Offset < Got.size());
		assert(Offset < Expected.size());
		assert(Got[Offset] == Expected[Offset]);
	}
}

#define AssertEquals(Got, Expected) AssertEquals_(Got, Expected, __LINE__, __FILE__)

struct BufferStream
{
	std::vector<uint8_t> &Buffer;
	size_t Offset;
	bool Dead;
	BufferStream(std::vector<uint8_t> &Buffer) : Buffer(Buffer), Offset(0), Dead(false) {}
	BufferStream &read(char *Out, size_t Length)
	{
		if (Offset + Length > Buffer.size()) { Dead = true; return *this; }
		memcpy(Out, &Buffer[Offset], Length);
		Offset += Length;
		return *this;
	}

	bool operator!(void) { return Dead; }
};

DefineProtocol(Proto1);
DefineProtocolVersion(Proto1_1, Proto1);
DefineProtocolMessage(Proto1_1_1, Proto1_1, void(int Val))
DefineProtocolMessage(Proto1_1_2, Proto1_1, void(unsigned int Val));
DefineProtocolMessage(Proto1_1_3, Proto1_1, void(uint64_t Val));
DefineProtocolMessage(Proto1_1_4, Proto1_1, void(bool Val));
DefineProtocolMessage(Proto1_1_5, Proto1_1, void(std::string Val));
DefineProtocolMessage(Proto1_1_6, Proto1_1, void(std::vector<uint8_t> Val));
DefineProtocolVersion(Proto1_2, Proto1);
DefineProtocolMessage(Proto1_2_1, Proto1_2, void(bool Space, int Val));
DefineProtocolMessage(Proto1_2_2, Proto1_2, void(bool Space, unsigned int Val));
DefineProtocolMessage(Proto1_2_3, Proto1_2, void(bool Space, uint64_t Val));
DefineProtocolMessage(Proto1_2_4, Proto1_2, void(bool Space, bool Val));
DefineProtocolMessage(Proto1_2_5, Proto1_2, void(bool Space, std::string Val));
DefineProtocolMessage(Proto1_2_6, Proto1_2, void(bool Space, std::vector<uint8_t> Val));

DefineProtocol(Proto2);
DefineProtocolVersion(Proto2_1, Proto2);
DefineProtocolMessage(Proto2_1_1, Proto2_1, void(int Val));

template<size_t Value> struct Overflow : std::integral_constant<size_t, Value + std::numeric_limits<unsigned char>::max() + 1> {};
static_assert(Proto1_1::ID == (Protocol::VersionIDType::Type)0, "ID calculation failed");
static_assert(Proto1_1_1::ID == (Protocol::MessageIDType::Type)0, "ID calculation failed");
static_assert(Proto1_1_2::ID == (Protocol::MessageIDType::Type)1, "ID calculation failed");
static_assert(Proto1_1_3::ID == (Protocol::MessageIDType::Type)2, "ID calculation failed");
static_assert(Proto1_1_4::ID == (Protocol::MessageIDType::Type)3, "ID calculation failed");
static_assert(Proto1_1_5::ID == (Protocol::MessageIDType::Type)4, "ID calculation failed");
static_assert(Proto1_1_6::ID == (Protocol::MessageIDType::Type)5, "ID calculation failed");
static_assert(Proto1_2::ID == (Protocol::VersionIDType::Type)1, "ID calculation failed");
static_assert(Proto1_2_1::ID == (Protocol::MessageIDType::Type)0, "ID calculation failed");
static_assert(Proto1_2_2::ID == (Protocol::MessageIDType::Type)1, "ID calculation failed");
static_assert(Proto1_2_3::ID == (Protocol::MessageIDType::Type)2, "ID calculation failed");
static_assert(Proto1_2_4::ID == (Protocol::MessageIDType::Type)3, "ID calculation failed");
static_assert(Proto1_2_5::ID == (Protocol::MessageIDType::Type)4, "ID calculation failed");
static_assert(Proto1_2_6::ID == (Protocol::MessageIDType::Type)5, "ID calculation failed");

int main(int argc, char **argv)
{
	std::vector<uint8_t> Buffer;
	StandardOutLog Log(__FILE__);
	Protocol::Reader
	<
		StandardOutLog,
		Proto1_1_1, Proto1_1_2, Proto1_1_3, Proto1_1_4, Proto1_1_5, Proto1_1_6,
		Proto1_2_1, Proto1_2_2, Proto1_2_3, Proto1_2_4, Proto1_2_5, Proto1_2_6
	>
	Reader1
	(
		Log,

		// Version 1
		[](int const &Val) { assert(Val == 11); },
		[](unsigned int const &Val) { assert(Val == 11); },
		[](uint64_t const &Val) { assert(Val == 11); },
		[](bool const &Val) { assert(Val == true); },
		[](std::string const &Val) { assert(Val == "dog"); },
		[](std::vector<uint8_t> const &Val)
		{
			assert(Val.size() == 3);
			assert(Val[0] == 0x00);
			assert(Val[1] == 0x01);
			assert(Val[2] == 0x02);
		},

		// Version 2
		[](bool const &Space, int const &Val) { assert(Space == true); assert(Val == 11); },
		[](bool const &Space, unsigned int const &Val) { assert(Space == true); assert(Val == 11); },
		[](bool const &Space, uint64_t const &Val) { assert(Space == true); assert(Val == 11); },
		[](bool const &Space, bool const &Val) { assert(Space == true); assert(Val == true); },
		[](bool const &Space, std::string const &Val) { assert(Space == true); assert(Val == "dog"); },
		[](bool const &Space, std::vector<uint8_t> const &Val)
		{
			assert(Space == true);
			assert(Val.size() == 3);
			assert(Val[0] == 0x00);
			assert(Val[1] == 0x01);
			assert(Val[2] == 0x02);
		}
	);

	Protocol::Reader
	<
		StandardOutLog,
		Proto1_1_1, Proto1_1_2, Proto1_1_3, Proto1_1_4, Proto1_1_5, Proto1_1_6,
		Proto1_2_1, Proto1_2_2, Proto1_2_3, Proto1_2_4, Proto1_2_5, Proto1_2_6
	>
	Reader2
	(
		Log,

		// Version 1
		[](int const &Val) { assert(Val == -4); },
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		[](std::vector<uint8_t> const &Val) { assert(Val.empty()); },

		// Version 2
		[](bool const &Space, int const &Val) { assert(Space == true); assert(Val == -4); },
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		[](bool const &Space, std::vector<uint8_t> const &Val) { assert(Val.empty()); }
	);

	// Proto 1
	Buffer = Proto1_1_1::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));
	Buffer = Proto1_1_1::Write(-4);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x00, 0x04, 0x00, 0xFC, 0xFF, 0xFF, 0xFF}));
	assert(Reader2.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_2::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x01, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_3::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x02, 0x08, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_4::Write(true);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x03, 0x01, 0x00, 0x01}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_5::Write("dog");
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x04, 0x05, 0x00, 0x03, 0x00, 'd', 'o', 'g'}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_6::Write(std::vector<uint8_t>{0x00, 0x01, 0x02});
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x05, 0x05, 0x00, 0x03, 0x00, 0x00, 0x01, 0x02}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_6::Write(std::vector<uint8_t>());
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x05, 0x02, 0x00, 0x00, 0x00}));
	assert(Reader2.Read(BufferStream{Buffer}));

	// Proto 1_2
	Buffer = Proto1_2_1::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x00, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));
	Buffer = Proto1_2_1::Write(true, -4);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x00, 0x05, 0x00, 0x01, 0xFC, 0xFF, 0xFF, 0xFF}));
	assert(Reader2.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_2::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x01, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_3::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x02, 0x09, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_4::Write(true, true);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x03, 0x02, 0x00, 0x01, 0x01}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_5::Write(true, "dog");
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x04, 0x06, 0x00, 0x01, 0x03, 0x00, 'd', 'o', 'g'}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_6::Write(true, std::vector<uint8_t>{0x00, 0x01, 0x02});
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x05, 0x06, 0x00, 0x01, 0x03, 0x00, 0x00, 0x01, 0x02}));
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_6::Write(true, std::vector<uint8_t>());
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x05, 0x03, 0x00, 0x01, 0x00, 0x00}));
	assert(Reader2.Read(BufferStream{Buffer}));

	// Proto 2_1
	int Mutate = 0;
	Buffer = Proto2_1_1::Write(45);
	Protocol::Reader<StandardOutLog, Proto2_1_1> Reader3(Log, [&](int const &Val) { Mutate = Val; });
	assert(Reader3.Read(BufferStream{Buffer}));
	assert(Mutate == 45);

	return 0;
}

