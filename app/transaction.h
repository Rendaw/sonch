#ifndef transaction_h
#define transaction_h

		typedef ProtocolClass StaticDataProtocol;
		typedef ProtocolMessageClass<ProtocolVersionClass<StaticDataProtocol>, 
			void(std::string InstanceName, UUID InstanceUUID)> StaticDataV1;
template <typename TransProto, typename TransVersion = ProtocolVersionClass<TransProto>> struct Transactor
{
	template <typename ...CallbackTypes> Transactor(std::mutex &CoreMutex, bfs::path const &TransactionPath, CallbackTypes const & ... Callbacks) : CoreMutex(CoreMutex)
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

	// TODO Act(Method)

	private:
		template <typename ...CallbackTypes> struct AddToReader {};
		template <typename ...TransactionType, typename ...RemainingCallbackTypes> struct AddToReader<std::function<Definition & ...>, RemainingCallbackTypes...>
		{
			AddToReader(Protocol::Reader<StandardOutLog> &Reader, std::function<void(TransactionType...)> const &Callback, RemainingCallbackTypes const & ... OtherCallbacks)
			{
				Reader.Add<TransVersion, TransactionType>(Callback);
				AddToReader<RemainingCallbackTypes...>(Reader, OtherCallbacks...);
			}
		};
		template <> struct AddToReader
		{
			AddToReader(Protocol::Reader<StandardOutLog> &Reader, std::function<void(TransactionType...)> const &Callback, RemainingCallbackTypes const & ... OtherCallbacks) {}
		};
};

#endif

