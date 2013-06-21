#ifndef protocol_h
#define protocol_h

// TODO Add conditional big endian -> little endian conversions

/*
Major versions are incompatible, and could be essentially different protocols.
 versions are compatible.

You may add minor versions to a protocol at any time, but you must never remove them.
Messages are arbitrary for any minor version, when that version is released.
*/

#include <vector>
#include <functional>
#include <limits>
#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>
#include <typeinfo>

#define ProtocolClass Protocol::Protocol_<__COUNTER__>::Type
#define ProtocolVersionClass Protocol::Version_<__COUNTER__>::Type
#define ProtocolMessageClass Protocol::Message_<__COUNTER__>::Type

template <template <typename...> class Destination> struct ApplyTypes
{
	template <typename Signature> struct Extract {};
	template <typename... Definition> struct Extract<void(Definition...)> : Destination<Definition...> { using Destination<Definition...>::Destination; };

	template <typename... Definition> struct Ref
	{
		template <typename Converted, typename... Unconverted> struct DestinationSpecializer {};

		template <typename... Converted, typename NextArgument, typename... RemainingArguments> 
			struct DestinationSpecializer<std::tuple<Converted...>, NextArgument, RemainingArguments...>
			{ typedef typename DestinationSpecializer<std::tuple<Converted..., NextArgument &>, RemainingArguments...>::Type Type; };

		template <typename... Converted> 
			struct DestinationSpecializer<std::tuple<Converted...>>
			{ typedef Destination<Converted...> Type; };

		typedef typename DestinationSpecializer<std::tuple<>, Definition...>::Type Type;
	};
};

template <typename RefType> struct Nonref {};
template <typename NonrefType> struct Nonref<NonrefType &> { typedef NonrefType Type; };

namespace Protocol
{

typedef uint8_t VersionIDType;
typedef uint8_t MessageIDType;
typedef uint16_t SizeType;
typedef uint16_t ArraySizeType;

template <typename ArrayType> ArraySizeType ConstrainArraySize(ArrayType const &Array)
{
	assert((Array.size() & ~0xffffffff) == 0);
	return std::min(static_cast<ArraySizeType>(Array.size()), std::numeric_limits<ArraySizeType>::max());
}

constexpr SizeType HeaderSize = sizeof(VersionIDType) + sizeof(MessageIDType) + sizeof(SizeType);

template <uint16_t Individuality> struct Protocol_
{
	struct Type
	{ 
		private:
			template <uint16_t VersionIndividuality> friend struct Version_;
			static VersionIDType VersionCount;
	};
};
template <uint16_t Individuality> VersionIDType Protocol_<Individuality>::Type::VersionCount = 0;

template <uint16_t Individuality> struct Version_
{
	template <typename InProtocol> struct Type
	{ 
		static VersionIDType const ID; 
		private:
			template <uint16_t MessageIndividuality> friend struct Message_;
			static MessageIDType MessageCount;
	};
};
template <uint16_t Individuality> template <typename InProtocol> VersionIDType const Version_<Individuality>::Type<InProtocol>::ID = InProtocol::VersionCount++;
template <uint16_t Individuality> template <typename InProtocol> MessageIDType Version_<Individuality>::Type<InProtocol>::MessageCount = 0;

template <uint16_t Individuality> struct Message_
{
	template <class InVersion, typename Definition> struct Type {};
	template <class InVersion, typename... Definition> struct Type<InVersion, void(Definition...)>
	{
		typedef InVersion Version;
		typedef void Signature(Definition...);
		static MessageIDType const ID;
		
		template <typename... ArgumentTypes> static std::vector<uint8_t> Write(ArgumentTypes... Arguments)
			{ return ConstArgumentConversion<std::tuple<>, Definition...>::Write(Arguments...); }

		private:
			template <typename Converted, typename... Unconverted> struct ConstArgumentConversion {};
			template <typename... Converted, typename NextArgument, typename... RemainingArguments> 
				struct ConstArgumentConversion<std::tuple<Converted...>, NextArgument, RemainingArguments...>
			{ 
				template <typename... ArgumentTypes> static std::vector<uint8_t> Write(ArgumentTypes... Arguments)
				{ 
					return ConstArgumentConversion<std::tuple<Converted..., NextArgument const &>, RemainingArguments...>::Write(Arguments...); 
				}
			};
			template <typename... Converted> struct ConstArgumentConversion<std::tuple<Converted...>>
			{ 
				static std::vector<uint8_t> Write(Converted... Arguments)
				{ 
					std::vector<uint8_t> Out;
					SizeType const DataSize = Size(Arguments...);
					Out.resize(HeaderSize + DataSize);
					uint8_t *WritePointer = &Out[0];
					Type<InVersion, void(Definition...)>::Write(WritePointer, InVersion::ID);
					Type<InVersion, void(Definition...)>::Write(WritePointer, ID);
					Type<InVersion, void(Definition...)>::Write(WritePointer, DataSize);
					Type<InVersion, void(Definition...)>::Write(WritePointer, Arguments...); 
					return Out;
				}
			};

			template <typename NextType, typename... RemainingTypes> 
				static inline SizeType Size(NextType NextArgument, RemainingTypes... RemainingArguments)
				{ return Size(NextArgument) + Size(RemainingArguments...); }

			static constexpr SizeType Size(void) { return 0; }

			template <typename NextType, typename... RemainingTypes>
				static inline void Write(uint8_t *&Out, NextType NextArgument, RemainingTypes... RemainingArguments)
				{
					Write(Out, NextArgument);
					Write(Out, RemainingArguments...);
				}

			static inline void Write(uint8_t *&) {}

			template <typename IntType> static constexpr SizeType Size(IntType const &Argument) 
				{ return sizeof(Argument); }
			template <typename IntType> static inline void Write(uint8_t *&Out, IntType const &Argument) 
				{ *reinterpret_cast<IntType *>(Out) = Argument; Out += sizeof(Argument); }

			static inline SizeType Size(std::string const &Argument) 
			{ 
				auto const Count = ConstrainArraySize(Argument);
				return sizeof(Count) + Count; 
			}
			static inline void Write(uint8_t *&Out, std::string const &Argument) 
			{ 
				auto const Count = ConstrainArraySize(Argument);
				Write(Out, Count); 
				memcpy(Out, Argument.c_str(), Count); 
				Out += Count;
			}
			
			template <typename ElementType, typename std::enable_if<!std::is_class<ElementType>::value>::type* = nullptr>
				static inline SizeType Size(std::vector<ElementType> const &Argument)
			{
				return sizeof(decltype(ConstrainArraySize(std::vector<int>()))) + Argument.size() * sizeof(ElementType);
			}
			template <typename ElementType, typename std::enable_if<!std::is_class<ElementType>::value>::type* = nullptr>
				static inline void Write(uint8_t *&Out, std::vector<ElementType> const &Argument)
			{
				auto const Count = ConstrainArraySize(Argument);
				Write(Out, Count);
				memcpy(Out, &Argument[0], Count * sizeof(ElementType));
				Out += Count * sizeof(ElementType);
			}

			template <typename ElementType, typename std::enable_if<std::is_class<ElementType>::value>::type* = nullptr>
				static inline SizeType Size(std::vector<ElementType> const &Argument)
			{
				auto const Count = ConstrainArraySize(Argument);
				SizeType Out = sizeof(Count);
				for (decltype(Count) ElementIndex = 0; ElementIndex < Argument.size(); ++ElementIndex)
					Out += Size(Argument[ElementIndex]);
				return Out;
			}
			template <typename ElementType, typename std::enable_if<std::is_class<ElementType>::value>::type* = nullptr>
				static inline void Write(uint8_t *&Out, std::vector<ElementType> const &Argument)
			{
				auto const Count = ConstrainArraySize(Argument);
				Write(Out, Count);
				for (decltype(Count) ElementIndex = 0; ElementIndex < Argument.size(); ++ElementIndex)
				{
					Write(Out, Argument[ElementIndex]);
				}
			}
	};
};
template <uint16_t Individuality> template <typename InVersion, typename... Definition> uint8_t const Message_<Individuality>::Type<InVersion, void(Definition...)>::ID = InVersion::MessageCount++;

template <typename LogType> class Reader
{
	typedef std::vector<uint8_t> BufferType;

	template <typename... Definition> struct CallbackGenerator
		{ typedef std::function<void(Definition...)> Type; };

	public:
		Reader(LogType &Log) : Log(Log) {}

		template <typename... Definition> using RefCallback = typename ApplyTypes<CallbackGenerator>::template Ref<Definition...>; // Couldn't get this to work when inlined
		template <typename MessageType> void Add(typename ApplyTypes<RefCallback>::template Extract<typename MessageType::Signature>::Type::Type Callback)
		{
			if (MessageType::Version::ID >= Versions.size())
			{
				if (MessageType::Version::ID == Versions.size())
					Versions.push_back(std::vector<std::unique_ptr<MessageReader>>());
				else assert(false); // Versions must be added sequentially to prevent gaps
			}

			if (MessageType::ID >= Versions[MessageType::Version::ID].size())
			{
				if (MessageType::ID == Versions[MessageType::Version::ID].size())
					Versions[MessageType::Version::ID].push_back(std::unique_ptr<MessageReader>(new typename ApplyTypes<ApplyTypes<MessageReaderImplementation>::template Ref>::template Extract<typename MessageType::Signature>::Type(Callback)));
				else assert(false); // Versions must be added sequentially to prevent gaps
			}
		}

		// StreamType must have ::read(char *, int) and operator! operators.
		template <typename StreamType> bool Read(StreamType &&Stream)
		{
			assert(!Versions.empty());
			Buffer.resize(HeaderSize);
			if (!Stream.read((char *)&Buffer[0], HeaderSize)) return false;
			VersionIDType const VersionID = *reinterpret_cast<VersionIDType *>(&Buffer[0]);
			if (VersionID >= Versions.size()) 
			{
				Log.Warn() << "Read message with invalid version: " << (unsigned int)VersionID << "; Highest known version is " << (unsigned int)Versions.size() - 1;
				return false;
			}
			MessageIDType const MessageID = *reinterpret_cast<MessageIDType *>(&Buffer[sizeof(VersionIDType)]);
			if (MessageID >= Versions[VersionID].size()) 
			{
				Log.Warn() << "Read message (version " << (unsigned int)VersionID << ") with invalid message type: " << (unsigned int)MessageID;
				return false;
			}
			SizeType const DataSize = *reinterpret_cast<SizeType *>(&Buffer[sizeof(VersionIDType) + sizeof(MessageIDType)]);
			Buffer.resize(DataSize);
			if (!Stream.read((char *)&Buffer[0], DataSize))
			{
				Log.Debug() << "End of file reached prematurely reading message body (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
				assert(false);
				return false;
			}
			return Versions[VersionID][MessageID]->Read(Log, VersionID, MessageID, Buffer);
		}
	private:
		LogType &Log;
		BufferType Buffer;

		struct MessageReader
		{
			virtual ~MessageReader(void) {}
			virtual bool Read(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer) = 0;
		};

		template <typename... Definition> struct MessageReaderImplementation : MessageReader
		{
			std::function<void(Definition...)> Callback;
			MessageReaderImplementation(std::function<void(Definition...)> const &Callback) : Callback(Callback) {}

			bool Read(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer) override
			{ 
				SizeType Offset = 0; 
				return ReadImplementation<std::tuple<Definition...>, std::tuple<>>::Read(*this, Log, VersionID, MessageID, Buffer, Offset); 
			}

			template <typename UnreadTypes, typename ReadTypes> struct ReadImplementation {};
			template <typename NextType, typename... RemainingTypes, typename... ReadTypes> 
				struct ReadImplementation<std::tuple<NextType, RemainingTypes...>, std::tuple<ReadTypes...>>
			{
				static bool Read(MessageReaderImplementation &This, LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, ReadTypes... ReadData)
				{
					typename Nonref<NextType>::Type Data;
					if (!This.ReadSingle(Log, VersionID, MessageID, Buffer, Offset, Data)) return false;
					return ReadImplementation<std::tuple<RemainingTypes...>, std::tuple<ReadTypes..., NextType>>::Read(This, Log, VersionID, MessageID, Buffer, Offset, ReadData..., Data);
				}
			};

			template <typename... ReadTypes> 
				struct ReadImplementation<std::tuple<>, std::tuple<ReadTypes...>>
			{
				static bool Read(MessageReaderImplementation &This, LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, ReadTypes... ReadData)
				{
					This.Callback(ReadData...);
					return true;
				}
			};

			template <typename IntType> bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, IntType &Data)
			{
				if (Buffer.size() < Offset + sizeof(IntType)) 
				{
					Log.Debug() << "End of file reached prematurely reading message body integer size " << sizeof(IntType) << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}

				Data = *reinterpret_cast<IntType const *>(&Buffer[Offset]);
				Offset += sizeof(IntType);
				return true;
			}
			
			bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, std::string &Data)
			{
				if (Buffer.size() < Offset + sizeof(ArraySizeType)) 
				{
					Log.Debug() << "End of file reached prematurely reading message body string header size " << sizeof(SizeType) << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				ArraySizeType const &Size = *reinterpret_cast<ArraySizeType const *>(&Buffer[Offset]);
				Offset += sizeof(Size);
				if (Buffer.size() < Offset + Size) 
				{
					Log.Debug() << "End of file reached prematurely reading message body string body size " << (unsigned int)Size << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				Data = std::string(reinterpret_cast<char const *>(&Buffer[Offset]), Size);
				Offset += Size;
				return true;
			}
			
			template <typename ElementType, typename std::enable_if<!std::is_class<ElementType>::value>::type* = nullptr>
			       	bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, std::vector<ElementType> &Data)
			{
				if (Buffer.size() < Offset + sizeof(ArraySizeType)) 
				{
					Log.Debug() << "End of file reached prematurely reading message body array header size " << sizeof(SizeType) << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				ArraySizeType const &Size = *reinterpret_cast<ArraySizeType const *>(&Buffer[Offset]);
				Offset += sizeof(Size);
				if (Buffer.size() < Offset + Size * sizeof(ElementType)) 
				{
					Log.Debug() << "End of file reached prematurely reading message body array body size " << (unsigned int)Size << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				Data.resize(Size);
				memcpy(&Data[0], &Buffer[Offset], Size * sizeof(ElementType));
				Offset += Size * sizeof(ElementType);
				return true;
			}
			
			template <typename ElementType, typename std::enable_if<std::is_class<ElementType>::value>::type* = nullptr>
			       	bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, std::vector<ElementType> &Data)
			{
				if (Buffer.size() < Offset + sizeof(ArraySizeType)) 
				{
					Log.Debug() << "End of file reached prematurely reading message body array header size " << sizeof(SizeType) << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				ArraySizeType const &Size = *reinterpret_cast<ArraySizeType const *>(&Buffer[Offset]);
				Offset += sizeof(Size);
				if (Buffer.size() < Offset + Size) 
				{
					Log.Debug() << "End of file reached prematurely reading message body array body size " << (unsigned int)Size << ", message length doesn't match contents (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
					assert(false);
					return false;
				}
				Data.resize(Size);
				for (ArraySizeType ElementIndex = 0; ElementIndex < Size; ++ElementIndex)
					ReadSingle(Log, VersionID, MessageID, Buffer, Offset, Data[ElementIndex]);
				return true;
			}
		};

		std::vector<std::vector<std::unique_ptr<MessageReader>>> Versions;
};

}

#endif

