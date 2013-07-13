#include <cstdint>
#include <iomanip>
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
	unsigned int Offset;
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

int main(int argc, char **argv)
{
	std::vector<uint8_t> Buffer;
	StandardOutLog Log(__FILE__);
	Protocol::Reader<StandardOutLog> Reader1(Log);
	Protocol::Reader<StandardOutLog> Reader2(Log);

	typedef ProtocolClass Proto1;
	typedef ProtocolVersionClass<Proto1> Proto1_1;
	typedef ProtocolMessageClass<Proto1_1, void(int Val)> Proto1_1_1;
	typedef ProtocolMessageClass<Proto1_1, void(unsigned int Val)> Proto1_1_2;
	typedef ProtocolMessageClass<Proto1_1, void(uint64_t Val)> Proto1_1_3;
	typedef ProtocolMessageClass<Proto1_1, void(bool Val)> Proto1_1_4;
	typedef ProtocolMessageClass<Proto1_1, void(std::string Val)> Proto1_1_5;
	typedef ProtocolMessageClass<Proto1_1, void(std::vector<uint8_t> Val)> Proto1_1_6;
	typedef ProtocolVersionClass<Proto1> Proto1_2;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, int Val)> Proto1_2_1;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, unsigned int Val)> Proto1_2_2;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, uint64_t Val)> Proto1_2_3;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, bool Val)> Proto1_2_4;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, std::string Val)> Proto1_2_5;
	typedef ProtocolMessageClass<Proto1_2, void(bool Space, std::vector<uint8_t> Val)> Proto1_2_6;
	
	// Proto 1
	Buffer = Proto1_1_1::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
	Reader1.Add<Proto1_1_1>([](int &Val) { assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));
	Buffer = Proto1_1_1::Write(-4);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x00, 0x04, 0x00, 0xFC, 0xFF, 0xFF, 0xFF}));
	Reader2.Add<Proto1_1_1>([](int &Val) { assert(Val == -4); });
	assert(Reader2.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_1_2::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x01, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
	assert(!Reader1.Read(BufferStream{Buffer}));
	Reader1.Add<Proto1_1_2>([](unsigned int &Val) { assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));

	static_assert(sizeof(uint64_t) == 8, "uint64_t is not 64 bits?");
	Buffer = Proto1_1_3::Write(11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x02, 0x08, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
	Reader1.Add<Proto1_1_3>([](uint64_t &Val) { assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_1_4::Write(true);
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x03, 0x01, 0x00, 0x01, }));
	Reader1.Add<Proto1_1_4>([](bool &Val) { assert(Val == true); });
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_1_5::Write("dog");
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x04, 0x05, 0x00, 0x03, 0x00, 'd', 'o', 'g'}));
	Reader1.Add<Proto1_1_5>([](std::string &Val) { assert(Val == "dog"); });
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_1_6::Write(std::vector<uint8_t>{0x00, 0x01, 0x02});
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x05, 0x05, 0x00, 0x03, 0x00, 0x00, 0x01, 0x02}));
	Reader1.Add<Proto1_1_6>([](std::vector<uint8_t> &Val) 
	{ 
		assert(Val.size() == 3); 
		assert(Val[0] == 0x00); 
		assert(Val[1] == 0x01);
		assert(Val[2] == 0x02);
	});
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_1_6::Write(std::vector<uint8_t>());
	AssertEquals(Buffer, std::vector<uint8_t>({0x00, 0x05, 0x02, 0x00, 0x00, 0x00}));
	Reader2.Add<Proto1_1_2>(nullptr);
	Reader2.Add<Proto1_1_3>(nullptr);
	Reader2.Add<Proto1_1_4>(nullptr);
	Reader2.Add<Proto1_1_5>(nullptr);
	Reader2.Add<Proto1_1_6>([](std::vector<uint8_t> &Val) { assert(Val.empty()); });
	assert(Reader2.Read(BufferStream{Buffer}));
	
	// Proto 1_2
	Buffer = Proto1_2_1::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x00, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
	Reader1.Add<Proto1_2_1>([](bool &Space, int &Val) { assert(Space == true); assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));
	Buffer = Proto1_2_1::Write(true, -4);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x00, 0x05, 0x00, 0x01, 0xFC, 0xFF, 0xFF, 0xFF}));
	Reader2.Add<Proto1_2_1>([](bool &Space, int &Val) { assert(Space == true); assert(Val == -4); });
	assert(Reader2.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_2_2::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x01, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
	Reader1.Add<Proto1_2_2>([](bool &Space, unsigned int &Val) { assert(Space == true); assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_3::Write(true, 11);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x02, 0x09, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
	Reader1.Add<Proto1_2_3>([](bool &Space, uint64_t &Val) { assert(Space == true); assert(Val == 11); });
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_2_4::Write(true, true);
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x03, 0x02, 0x00, 0x01, 0x01}));
	Reader1.Add<Proto1_2_4>([](bool &Space, bool &Val) { assert(Space == true); assert(Val == true); });
	assert(Reader1.Read(BufferStream{Buffer}));

	Buffer = Proto1_2_5::Write(true, "dog");
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x04, 0x06, 0x00, 0x01, 0x03, 0x00, 'd', 'o', 'g'}));
	Reader1.Add<Proto1_2_5>([](bool &Space, std::string &Val) { assert(Space == true); assert(Val == "dog"); });
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_2_6::Write(true, std::vector<uint8_t>{0x00, 0x01, 0x02});
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x05, 0x06, 0x00, 0x01, 0x03, 0x00, 0x00, 0x01, 0x02}));
	Reader1.Add<Proto1_2_6>([](bool &Space, std::vector<uint8_t> &Val) 
	{ 
		assert(Space == true);
		assert(Val.size() == 3); 
		assert(Val[0] == 0x00); 
		assert(Val[1] == 0x01);
		assert(Val[2] == 0x02);
	});
	assert(Reader1.Read(BufferStream{Buffer}));
	
	Buffer = Proto1_2_6::Write(true, std::vector<uint8_t>());
	AssertEquals(Buffer, std::vector<uint8_t>({0x01, 0x05, 0x03, 0x00, 0x01, 0x00, 0x00}));
	Reader2.Add<Proto1_2_2>(nullptr);
	Reader2.Add<Proto1_2_3>(nullptr);
	Reader2.Add<Proto1_2_4>(nullptr);
	Reader2.Add<Proto1_2_5>(nullptr);
	Reader2.Add<Proto1_2_6>([](bool &Space, std::vector<uint8_t> &Val) { assert(Val.empty()); });
	assert(Reader2.Read(BufferStream{Buffer}));

	return 0;
}

