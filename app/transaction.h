#ifndef transaction_h
#define transaction_h

template <typename TransProto, typename TransVersion = ProtocolVersionClass<TransProto>> struct Transactor
{
	template <typename ...CallbackTypes> Transactor(std::mutex &CoreMutex, bfs::path const &TransactionPath, CallbackTypes const & ... Callbacks) : CoreMutex(CoreMutex), TransactionPath(TransactionPath)
	{
		StandardOutLog Log("transaction recovery");
		Protocol::Reader<StandardOutLog> Reader(Log);
		AddToReader(Reader, Callbacks...);
		for (bfs::directory_iterator Filename(TransactionPath); Filename != bfs::directory_iterator(); ++Filename)
		{
			{
				std::lock_guard Guard(CoreMutex);
				bfs::ifstream In(*Filename, std::ifstream::in | std::ifstream::binary);
				bool Success = Reader.Read(In);
				if (!Success)
					throw SystemError() << "Could not read transaction file '" << *Filename << "', file may be corrupt.";
			}
			bfs::remove(*Filename);
		}
	}

	template <typename ...ArgumentTypes> void Act(void (*Callback)(ArgumentTypes const &...), ArgumentTypes const &... Arguments)
	{
		bfs::path ThreadPath = TransactionPath / std::this_thread::get_id();
		bfs::ofstream Out(ThreadPath, std::ofstream::out | std::ofstream::binary);
		if (!Out) throw SystemError() << "Could not create file '" << ThreadPath << "'.";
		auto const &Data = ProtocolMessageClass<TransVersion, void(ArgumentTypes ...)>::Write(Arguments...);
		Out.write((char const *)&Data[0], Data.size());
		{
			std::lock_guard Guard(CoreMutex);
			Callback(Arguments...);
		}
		bfs::remove(ThreadPath / std::this_thread::get_id());
	}

	private:
		template <typename ...CallbackTypes> struct Register {};
		template <typename ...TransactionType, typename ...RemainingCallbackTypes> struct Register<std::function<Definition & ...>, RemainingCallbackTypes...>
		{
			Register(Protocol::Reader<StandardOutLog> &Reader, std::function<void(TransactionType...)> const &Callback, RemainingCallbackTypes const & ... OtherCallbacks)
			{
				Reader.Add<TransVersion, TransactionType>(Callback);
				Register<RemainingCallbackTypes...>(Reader, OtherCallbacks...);
			}
		};
		template <> struct Register
		{
			Register(Protocol::Reader<StandardOutLog> &Reader, std::function<void(TransactionType...)> const &Callback, RemainingCallbackTypes const & ... OtherCallbacks) {}
		};

		std::mutex &CoreMutex;
		bfs::path const &TransactionPath;
};

#endif

