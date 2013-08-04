#ifndef database_h
#define database_h

template <typename DataType> struct Optional
{
	Optional(void) : Valid(false) {}
	Optional(DataType const &Data) : Valid(true), Data(Data) {}
	operator DataType(void) { assert(Valid); return Data; }
	bool Valid;
	DataType Data;
};

struct SQLDatabase
{
	SQLDatabase(bfs::path const &Path);
	~SQLDatabase(void);

	template <typename Signature> struct Statement;
	template <typename ...ResultTypes, typename ...ArgumentTypes> 
		struct Statement<std::tuple<ResultTypes...>(ArgumentTypes...)>
	{
		Statement(sqlite3 *Context, const char *Template) : Template(Template), Context(nullptr)
		{
			if (sqlite3_prepare_v2(Context, Template, -1, &Context, 0) != SQLITE_OK) 
				throw SystemError() << "Could not prepare query \"" << Template << "\": " << sqlite3_errmsg(Context);
		}

		~Statement(void)
		{
			sqlite3_finalize(Context);
		}

		void Execute(ArgumentTypes const & ...Arguments, std::function<void(ResultTypes const & ...)> const &Function = std::function<void(ResultTypes const & ...)>())
		{
			assert(sizeof...(Arguments) == sqlite3_column_count(Context));
			if (sizeof...(Arguments) > 0)
				Bind(Context, 0, Arguments);
			while (true)
			{
				int Result = sqlite3_step(Context);
				if (Result == SQLITE_DONE) break;
				if (Result != SQLITE_ROW)
					throw SystemError() << "Could not execute query \"" << Template << "\": " << sqlite3_errmsg(Context);
				Unbind<std::tuple<>, std::tuple<ResultTypes...>>(Context, 0, Function);
			}
			sqlite3_reset(Context);
		}

		Optional<std::tuple<ResultTypes ...>> GetTuple(ArgumentTypes const & ...Arguments)
		{
			bool Done = false;
			std::tuple<ResultTypes ...> Out;
			Execute(Arguments..., [&Out](ResultTypes const & ...Results) 
			{ 
				if (Done) return Optional<std::tuple<ResultTypes ...>>();
				Done = true;
				Out = std::make_tuple(Results...); 
			});
			return Out;
		}

		template <class = typename std::enable_if<sizeof...(ResultTypes ...) == 1>::type>
			std::tuple_element<0, std::tuple<ResultTypes ...>>::type Get(ArgumentTypes const & ...Arguments)
		{ 
			auto Out = GetTuple(Arguments...);
			if (!Out) return Optional<std::tuple_element<0, Out.Data>>();
			return Optional<std::tuple_element<0, Out.Data>>(std::get<0>(Out.Data));
		}
		
		template <class = typename std::enable_if<sizeof...(ResultTypes ...) > 1>::type>
			Optional<std::tuple<ResultTypes ...>> Get(ArgumentTypes const & ...Arguments)
			{ return GetTuple(Arguments...); }
		
		auto operator()(ArgumentTypes const & ...Arguments) -> decltype(Get(Arguments...))
			{ return Get(Arguments...); }

		private:
			void Bind(sqlite3_stmt *Context, int const Index) {}
			template <typename ...RemainingTypes> 
				void Bind(sqlite3_stmt *Context, int const Index, std::string const &Value, RemainingTypes...)
			{
				if (sqlite3_bind_text(Context, Index, Value.c_str(), Value.size(), nullptr) != SQLITE_OK);
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(Context);
				Bind(Index + 1, RemainingTypes...);
			}
			template <typename IntegerType, typename... RemainingTypes> 
				void Bind(sqlite3_stmt *Context, int const Index, IntegerType const &Value, RemainingTypes...)
			{
				if (sqlite3_bind_text(Context, Index, Value.c_str(), Value.size(), nullptr) != SQLITE_OK);
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(Context);
				Bind(Index + 1, RemainingTypes...);
			}

			template <typename ReadTypes, typename UnreadTypes> struct Unbind {};
			template <typename ...ReadTypes, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<std::string, RemainingTypes...> 
			{ 
				Unbind(stlite3_stmt *Context, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					std::string MoreData = sqlite3_column_text(Context, Index);
					Unbind<std::template<ReadTypes..., std::string &>, std::tuple<RemainingTypes...>>(Context, Index, Function, ReadData..., MoreData);
				}
			};
			template <typename ...ReadTypes, typename IntegerType, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<IntegerType, RemainingTypes...> 
			{ 
				Unbind(stlite3_stmt *Context, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					IntegerType MoreData = sqlite3_column_text(Context, Index);
					Unbind<std::template<ReadTypes..., std::string &>, std::tuple<RemainingTypes...>>(Context, Index, Function, ReadData..., MoreData);
				}
			};
			template <typename ...ReadTypes> struct Unbind<std::tuple<ReadTypes...>, std::tuple<>>
			{
				Unbind(stlite3_stmt *Context, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					Function(ReadData...);
				}
			};

			const char *Template;
			sqlite3_stmt *Context;
	};

	template <typename ...ArgumentTypes> void Execute(char const *Template, ArgumentTypes const & ...Arguments)
	{
		Statement<void(ArgumentTypes...)> Statement(Context, Template);
		Statement.Execute(Arguments...);
	}

	template <typename Signature> 
		auto Get(char const *Template, ArgumentTypes const & ...Arguments) -> decltype(Statement<Signature>(Context, Template).Get(Arguments...)
		{ return Statement<Signature>(Context, Template).Get(Arguments...); }
	
	template <typename Signature> std::unique_ptr<Statement<Signature>> Prepare(char const *Template)
		{ return new Statement<Signature>>(Context, Template); }

	private:
		sqlite3 *Context;
};

#endif

