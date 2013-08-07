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

template <typename TargetType> struct Type {};

template <typename ...MessageTypes> struct Transactor
{
	template <typename ...CallbackTypes> Transactor(std::mutex &CoreMutex, bfs::path const &TransactionPath, CallbackTypes ...Callbacks) : CoreMutex(CoreMutex), TransactionPath(TransactionPath), Log("transaction recovery"), Reader(Log)
	{
		Register<void, std::tuple<MessageTypes...>, std::tuple<CallbackTypes...>>(Reader, Callbacks...);
		for (bfs::directory_iterator Filename(TransactionPath); Filename != bfs::directory_iterator(); ++Filename)
		{
			{
				std::lock_guard<std::mutex> Guard(CoreMutex);
				bfs::ifstream In(*Filename, std::ifstream::in | std::ifstream::binary);
				bool Success = Reader.Read(In);
				if (!Success)
					throw SystemError() << "Could not read transaction file '" << *Filename << "', file may be corrupt.";
			}
			bfs::remove(*Filename);
		}
	}

	template <typename MessageType, typename ...ArgumentTypes> void Act(Type<MessageType>, ArgumentTypes const &... Arguments)
	{
		static_assert(TypeIn<MessageType, MessageTypes...>::Value, "MessageType is unregisteed.  Type must be registered with callback in constructor.");
		bfs::path ThreadPath = TransactionPath / (std::string)(String() << std::this_thread::get_id());
		bfs::ofstream Out(ThreadPath, std::ofstream::out | std::ofstream::binary);
		if (!Out) throw SystemError() << "Could not create file '" << ThreadPath << "'.";
		auto const &Data = MessageType::Write(Arguments...);
		Out.write((char const *)&Data[0], Data.size());
		{
			std::lock_guard<std::mutex> Guard(CoreMutex);
			Reader.Call<MessageType>(Arguments...);
		}
		bfs::remove(ThreadPath / (std::string)(String() << std::this_thread::get_id()));
	}
	
	template <typename MessageType, typename ...ArgumentTypes> void operator()(Type<MessageType> Type, ArgumentTypes const &... Arguments)
		{ Act(Type, std::forward<ArgumentTypes const>(Arguments)...); }
	
	private:
		template <typename Dummy, typename RemainingMessageTypes, typename RemainingCallbackTypes> struct Register {};
		template <typename MessageType, typename ...RemainingMessageTypes, typename CallbackType, typename ...RemainingCallbackTypes> struct Register<void, std::tuple<MessageType, RemainingMessageTypes...>, std::tuple<CallbackType, RemainingCallbackTypes...>>
		{
			Register(Protocol::Reader<StandardOutLog> &Reader, CallbackType const &Callback, RemainingCallbackTypes ...OtherCallbacks)
			{
				Reader.Add<MessageType>(Callback);
				Register<void, std::tuple<RemainingMessageTypes...>, std::tuple<RemainingCallbackTypes...>>(Reader, OtherCallbacks...);
			}
		};
		template <typename Dummy> struct Register<Dummy, std::tuple<>, std::tuple<>>
		{
			Register(Protocol::Reader<StandardOutLog> &Reader) {}
		};

		std::mutex &CoreMutex;
		bfs::path const &TransactionPath;
		StandardOutLog Log;
		Protocol::Reader<StandardOutLog> Reader;
};

#endif

