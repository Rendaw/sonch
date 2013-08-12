#ifndef protocol_h
#define protocol_h

// TODO Add conditional big endian -> little endian conversions

/*
Major versions are incompatible, and could be essentially different protocols.
 versions are compatible.

You may add minor versions to a protocol at any time, but you must never remove them.
Messages are completely redefined for each new version.
*/

#include <vector>
#include <functional>
#include <limits>
#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>
#include <typeinfo>

#include "constcount.h"

#define DefineProtocol(Name) typedef Protocol::Protocol<__COUNTER__> Name;
#define DefineProtocolVersion(Name, InProtocol) typedef Protocol::Version<GetConstCount(InProtocol), InProtocol> Name; IncrementConstCount(InProtocol);
#define DefineProtocolMessage(Name, InVersion, Signature) typedef Protocol::Message<GetConstCount(InVersion), InVersion, Signature> Name; IncrementConstCount(InVersion);

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

template <uint16_t Individuality> struct Protocol {};

template <VersionIDType IDValue, typename InProtocol> struct Version
	{ static constexpr VersionIDType ID = IDValue; };
template <VersionIDType IDValue, typename InProtocol> constexpr VersionIDType Version<IDValue, InProtocol>::ID;

template <MessageIDType, typename, typename> struct Message;
template <MessageIDType IDValue, typename InVersion, typename ...Definition> struct Message<IDValue, InVersion, void(Definition...)>
{
	typedef InVersion Version;
	typedef void Signature(Definition...);
	typedef std::function<void(Definition const &...)> Function;
	static constexpr MessageIDType ID = IDValue;

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
				Message<ID, InVersion, void(Definition...)>::Write(WritePointer, InVersion::ID);
				Message<ID, InVersion, void(Definition...)>::Write(WritePointer, ID);
				Message<ID, InVersion, void(Definition...)>::Write(WritePointer, DataSize);
				Message<ID, InVersion, void(Definition...)>::Write(WritePointer, Arguments...);
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
template <MessageIDType IDValue, typename InVersion, typename ...Definition> constexpr MessageIDType Message<IDValue, InVersion, void(Definition...)>::ID;

template <size_t CurrentVersionID, size_t CurrentMessageID, typename Enabled, typename ...MessageTypes> struct ReaderTupleElement;
template
<
	size_t CurrentVersionID,
	size_t CurrentMessageID,
	typename MessageType,
	typename ...RemainingMessageTypes
>
struct ReaderTupleElement
<
	CurrentVersionID,
	CurrentMessageID,
	typename std::enable_if<(CurrentVersionID == MessageType::Message::Version::ID) && (CurrentMessageID == MessageType::Message::ID)>::type,
	MessageType, RemainingMessageTypes...
>
: ReaderTupleElement
<
	CurrentVersionID,
	CurrentMessageID + 1,
	void,
	RemainingMessageTypes...
>
{
	private:
		template <typename ParentType = MessageType> struct MessageDerivedTypes;
		template <MessageIDType ID, typename InVersion, typename ...Definition>
			struct MessageDerivedTypes<Message<ID, InVersion, void(Definition...)>>
		{
			typedef std::tuple<Definition...> Tuple;
			typedef std::function<void(Definition const &...)> Callback;
		};

		typedef ReaderTupleElement
		<
			CurrentVersionID,
			CurrentMessageID,
			void,
			MessageType, RemainingMessageTypes...
		> ThisElement;

		typedef ReaderTupleElement
		<
			CurrentVersionID,
			CurrentMessageID + 1,
			void,
			RemainingMessageTypes...
		> NextElement;

		typedef std::vector<uint8_t> BufferType;

		typename MessageDerivedTypes<>::Callback Callback;

	protected:
		template <typename CallbackType, typename ...RemainingCallbackTypes>
			ReaderTupleElement(CallbackType const &Callback, RemainingCallbackTypes const &...RemainingCallbacks) :
			NextElement(std::forward<RemainingCallbackTypes const &>(RemainingCallbacks)...), Callback(Callback)
			{}

		template <typename LogType>
			bool Read(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer)
		{
			if ((VersionID == MessageType::Version::ID) && (MessageID == MessageType::ID))
			{
				SizeType Offset = 0;
				return ReadImplementation<LogType, typename MessageDerivedTypes<>::Tuple, std::tuple<>>::Read(*this, Log, VersionID, MessageID, Buffer, Offset);
			}
			return NextElement::Read(Log, VersionID, MessageID, Buffer);
		};

	public:
		template <
			typename CallMessageType,
			typename ...ArgumentTypes,
			typename std::enable_if<std::is_same<CallMessageType, MessageType>::value>::type* = nullptr>
			void Call(ArgumentTypes const & ...Arguments)
			{ Callback(std::forward<ArgumentTypes const &>(Arguments)...); };

		template <
			typename CallMessageType,
			typename ...ArgumentTypes,
			typename std::enable_if<!std::is_same<CallMessageType, MessageType>::value>::type* = nullptr>
			void Call(ArgumentTypes const & ...Arguments)
			{ NextElement::template Call<CallMessageType, ArgumentTypes...>(std::forward<ArgumentTypes const &>(Arguments)...); };

	private:
		template <typename LogType, typename UnreadTypes, typename ReadTypes> struct ReadImplementation {};
		template <typename LogType, typename NextType, typename... RemainingTypes, typename... ReadTypes>
			struct ReadImplementation<LogType, std::tuple<NextType, RemainingTypes...>, std::tuple<ReadTypes...>>
		{
			static bool Read(ThisElement &This, LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, ReadTypes const &...ReadData)
			{
				NextType Data;
				if (!This.ReadSingle(Log, VersionID, MessageID, Buffer, Offset, Data)) return false;
				return ReadImplementation<LogType, std::tuple<RemainingTypes...>, std::tuple<ReadTypes..., NextType>>::Read(This, Log, VersionID, MessageID, Buffer, Offset, std::forward<ReadTypes const &>(ReadData)..., std::move(Data));
			}
		};

		template <typename LogType, typename... ReadTypes>
			struct ReadImplementation<LogType, std::tuple<>, std::tuple<ReadTypes...>>
		{
			static bool Read(ThisElement &This, LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, ReadTypes const &...ReadData)
			{
				This.Callback(std::forward<ReadTypes const &>(ReadData)...);
				return true;
			}
		};

		template <typename LogType, typename IntType> bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, IntType &Data)
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

		template <typename LogType> bool ReadSingle(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer, SizeType &Offset, std::string &Data)
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

		template <typename LogType, typename ElementType, typename std::enable_if<!std::is_class<ElementType>::value>::type* = nullptr>
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

		template <typename LogType, typename ElementType, typename std::enable_if<std::is_class<ElementType>::value>::type* = nullptr>
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

template
<
	size_t CurrentVersionID,
	size_t CurrentMessageID,
	typename MessageType,
	typename ...RemainingMessageTypes
>
struct ReaderTupleElement
<
	CurrentVersionID,
	CurrentMessageID,
	typename std::enable_if<(CurrentVersionID + 1 == MessageType::Message::Version::ID)>::type,
	MessageType, RemainingMessageTypes...
>
: ReaderTupleElement
<
	CurrentVersionID + 1,
	0,
	void,
	MessageType, RemainingMessageTypes...
>
{
	typedef ReaderTupleElement
	<
		CurrentVersionID + 1,
		0,
		void,
		MessageType, RemainingMessageTypes...
	> NextElement;

	template <typename ...RemainingCallbackTypes>
		ReaderTupleElement(RemainingCallbackTypes const &...RemainingCallbacks) :
		NextElement(std::forward<RemainingCallbackTypes const &>(RemainingCallbacks)...)
		{}
};

template
<
	size_t CurrentVersionID,
	size_t CurrentMessageID
>
struct ReaderTupleElement
<
	CurrentVersionID,
	CurrentMessageID,
	void
>
{
	typedef std::vector<uint8_t> BufferType;

	template <typename LogType> bool Read(LogType &Log, VersionIDType const &VersionID, MessageIDType const &MessageID, BufferType const &Buffer)
	{
		Log.Warn() << "Read message with invalid version or message type (version " << (unsigned int)VersionID << ") with invalid message type: " << (unsigned int)MessageID;
		assert(false);
		return false;
	};
};

template <typename LogType, typename ...MessageTypes> struct Reader : ReaderTupleElement<0, 0, void, MessageTypes...>
{
	template <typename ...CallbackTypes> Reader(LogType &Log, CallbackTypes const &...Callbacks) : HeadElement(Callbacks...), Log(Log) {}

	// StreamType must have ::read(char *, int) and operator! operators.
	template <typename StreamType> bool Read(StreamType &&Stream)
	{
		Buffer.resize(HeaderSize);
		if (!Stream.read((char *)&Buffer[0], HeaderSize)) return true;
		VersionIDType const VersionID = *reinterpret_cast<VersionIDType *>(&Buffer[0]);
		MessageIDType const MessageID = *reinterpret_cast<MessageIDType *>(&Buffer[sizeof(VersionIDType)]);
		SizeType const DataSize = *reinterpret_cast<SizeType *>(&Buffer[sizeof(VersionIDType) + sizeof(MessageIDType)]);
		Buffer.resize(DataSize);
		if (!Stream.read((char *)&Buffer[0], DataSize))
		{
			Log.Debug() << "End of file reached prematurely reading message body (version " << (unsigned int)VersionID << ", type " << (unsigned int)MessageID << ")";
			assert(false);
			return false;
		}
		return HeadElement::Read(Log, VersionID, MessageID, Buffer);
	}

	private:
		typedef ReaderTupleElement<0, 0, void, MessageTypes...> HeadElement;

		LogType &Log;
		std::vector<uint8_t> Buffer;
};

}

#endif

