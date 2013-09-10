#ifndef transaction_h
#define transaction_h

#include "protocol.h"
#include "log.h"
#include "shared.h"

#include <mutex>
#include <thread>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bfs = boost::filesystem;

template <typename Type, typename ...TypeSet> struct TypeIn;
template <typename Type, typename ...RemainingTypes> struct TypeIn<Type, Type, RemainingTypes...>
	{ static constexpr bool Value = true; };
template <typename Type> struct TypeIn<Type>
	{ static constexpr bool Value = false; };
template <typename Type, typename OtherType, typename ...RemainingTypes> struct TypeIn<Type, OtherType, RemainingTypes...> : TypeIn<Type, RemainingTypes...> {};

template <typename ...MessageTypes> struct Transactor
{
	template <typename ...CallbackTypes> Transactor(bfs::path const &TransactionPath, CallbackTypes ...Callbacks) : TransactionPath(TransactionPath), Log("transaction recovery"), Reader(Log, std::forward<CallbackTypes>(Callbacks)...)
	{
		for (bfs::directory_iterator Filename(TransactionPath); Filename != bfs::directory_iterator(); ++Filename)
		{
			{
				bfs::ifstream In(*Filename, std::ifstream::in | std::ifstream::binary);
				while (In)
				{
					bool Success = Reader.Read(In);
					if (!Success)
						throw SystemError() << "Could not read transaction file " << *Filename << ", file may be corrupt.";
				}
			}
			bfs::remove(*Filename);
		}
	}

	template <typename MessageType, typename ...ArgumentTypes> void Act(ArgumentTypes const &... Arguments)
	{
		static_assert(TypeIn<MessageType, MessageTypes...>::Value, "MessageType is unregisteed.  Type must be registered with callback in constructor.");
		bfs::path ThreadPath = TransactionPath / (std::string)(String() << std::this_thread::get_id());
		bfs::ofstream Out(ThreadPath, std::ofstream::out | std::ofstream::binary);
		if (!Out) throw SystemError() << "Could not create file " << ThreadPath << ".";
		auto const &Data = MessageType::Write(Arguments...);
		Out.write((char const *)&Data[0], static_cast<std::streamsize>(Data.size()));
		Reader.template Call<MessageType>(std::forward<ArgumentTypes const &>(Arguments)...);
		bfs::remove(ThreadPath / (std::string)(String() << std::this_thread::get_id()));
	}

	template <typename MessageType, typename ...ArgumentTypes> void operator()(MessageType, ArgumentTypes const &... Arguments)
		{ Act<MessageType>(std::forward<ArgumentTypes const &>(Arguments)...); }

	private:
		bfs::path const TransactionPath;
		StandardOutLog Log;
		Protocol::Reader<StandardOutLog, MessageTypes...> Reader;
};

#endif

