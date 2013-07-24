#ifndef database_h
#define database_h

struct SQLDatabase
{
	SQLDatabase(bfs::path const &Path);
	~SQLDatabase(void);

	template <typename ArgumentTypes, typename ResultTypes> struct Statement;
	template <typename ...ArgumentTypes, typename ...ResultTypes> 
		struct Statement<std::tuple<ArgumentTypes...>, std::tuple<ResultTypes...>>
	{
		Statement(sqlite3 *Context, const char *Template) : Template(Template), Statement(nullptr)
		{
			if (sqlite3_prepare_v2(Context, Template, -1, &Statement, 0) != SQLITE_OK) 
				throw SystemError() << "Could not prepare query \"" << Template << "\": " << sqlite3_errmsg(Context);
		}

		~Statement(void)
		{
			sqlite3_finalize(Statement);
		}

		void Execute(ArgumentTypes const & ...Arguments, std::function<void(ResultTypes const & ...)> const &Function = std::function<void(ResultTypes const & ...)>())
		{
			assert(sizeof...(Arguments) == sqlite3_column_count(Statement));
			if (sizeof...(Arguments) > 0)
				Bind(Statement, 0, Arguments);
			while (true)
			{
				int Result = sqlite3_step(Statement);
				if (Result == SQLITE_DONE) break;
				if (Result != SQLITE_ROW)
					throw SystemError() << "Could not execute query \"" << Template << "\": " << sqlite3_errmsg(Context);
				Unbind<std::tuple<>, std::tuple<ResultTypes...>>(Statement, 0, Function);
			}
			sqlite3_reset(Statement);
		}

		std::tuple<ResultTypes ...> GetTuple(ArgumentTypes const & ...Arguments)
		{
			bool Done = false;
			std::tuple<ResultTypes ...> Out;
			Execute(Arguments..., [&Out](ResultTypes const & ...Results) 
			{ 
				if (Done) return;
				Done = true;
				Out = std::make_tuple(Results...); 
			});
			return Out;
		}
		
		template <class = typename std::enable_if<sizeof...(ResultTypes ...) == 1>::type>
			std::tuple_element<0, std::tuple<ResultTypes ...>>::type Get(ArgumentTypes const & ...Arguments)
			{ return std::get<0>(GetTuple(Arguments...)); }
		
		template <class = typename std::enable_if<sizeof...(ResultTypes ...) > 1>::type>
			std::tuple<ResultTypes ...> Get(ArgumentTypes const & ...Arguments)
			{ return GetTuple(Arguments...); }

		private:
			void Bind(sqlite3_stmt *Statement, int const Index) {}
			template <typename ...RemainingTypes> 
				void Bind(sqlite3_stmt *Statement, int const Index, std::string const &Value, RemainingTypes...)
			{
				if (sqlite3_bind_text(Statement, Index, Value.c_str(), Value.size(), nullptr) != SQLITE_OK);
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(Context);
				Bind(Index + 1, RemainingTypes...);
			}
			template <typename IntegerType, typename... RemainingTypes> 
				void Bind(sqlite3_stmt *Statement, int const Index, IntegerType const &Value, RemainingTypes...)
			{
				if (sqlite3_bind_text(Statement, Index, Value.c_str(), Value.size(), nullptr) != SQLITE_OK);
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(Context);
				Bind(Index + 1, RemainingTypes...);
			}

			template <typename ReadTypes, typename UnreadTypes> struct Unbind {};
			template <typename ...ReadTypes, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<std::string, RemainingTypes...> 
			{ 
				Unbind(stlite3_stmt *Statement, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					std::string MoreData = sqlite3_column_text(Statement, Index);
					Unbind<std::template<ReadTypes..., std::string &>, std::tuple<RemainingTypes...>>(Statement, Index, Function, ReadData..., MoreData);
				}
			};
			template <typename ...ReadTypes, typename IntegerType, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<IntegerType, RemainingTypes...> 
			{ 
				Unbind(stlite3_stmt *Statement, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					IntegerType MoreData = sqlite3_column_text(Statement, Index);
					Unbind<std::template<ReadTypes..., std::string &>, std::tuple<RemainingTypes...>>(Statement, Index, Function, ReadData..., MoreData);
				}
			};
			template <typename ...ReadTypes> struct Unbind<std::tuple<ReadTypes...>, std::tuple<>>
			{
				Unbind(stlite3_stmt *Statement, int const Index, std::function<void(ResultTypes const & ...)> const &Function, ReadTypes ...ReadData)
				{
					Function(ReadData...);
				}
			};

			const char *Template;
			sqlite3_stmt *Statement;
	};

	// Usage: Database.Execute("STATEMENT ? ?", x, y);
	template <typename ...ArgumentTypes> void Execute(char const *Template, ArgumentTypes const & ...Arguments)
	{
		Statement<std::tuple<ArgumentTypes...>, std::tuple<>> Statement(Context, Template);
		Statement.Execute(Arguments...);
	}

	// Usage: X Result = Database.Get<std::tuple<X>>("STATEMENT ?", z);
	template <typename ResultTypes, typename ...ArgumentTypes, class = typename std::enable_if<std::tuple_size<ResultTypes>::value == 1>::type>
		std::tuple_element<0, ResultTypes>::type Get(ArgumentTypes const & ...Arguments)
	{ 
		Statement<std::tuple<ArgumentTypes...>, ResultTypes...> Statement(Context, Template);
		return Statement.Get(Arguments...);
	}
	
	// Usage: std::tuple<X, Y> Result = Database.Get<std::tuple<X, Y>>("STATEMENT ?", z);
	template <typename ResultTypes, typename ...ArgumentTypes, class = typename std::enable_if<std::tuple_size<ResultTypes>::value > 1>::type>
		ResultTypes Get(ArgumentTypes const & ...Arguments)
	{ 
		Statement<std::tuple<ArgumentTypes...>, std::tuple<>> Statement(Context, Template);
		return Statement.Get(Arguments...);
	}
	
	// Usage: auto Statement = Database.Prepare<std::tuple<X, Y>, std::tuple<Z>>("STATEMENT");
	template <typename ArgumentTypes, typename ResultTypes = std::tuple<>> std::unique_ptr<Statement<ArgumentTypes, ResultTypes>> Prepare(char const *Template)
		{ return new Statement<ArgumentTypes, ResultTypes>>(Context, Template); }

	private:
		sqlite3 *Context;
};

#endif

