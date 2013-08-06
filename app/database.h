#ifndef database_h
#define database_h

#include "error.h"

#include <sqlite3.h>
#include <cassert>
#include <boost/filesystem.hpp>
#include <type_traits>

namespace bfs = boost::filesystem;

template <typename DataType> struct Optional
{
	Optional(void) : Valid(false) {}
	Optional(DataType const &Data) : Valid(true), Data(Data) {}
	operator bool(void) { return Valid; }
	bool operator !(void) { return !Valid; }
	DataType &operator *(void) { assert(Valid); return Data; }
	DataType &operator ->(void) { assert(Valid); return Data; }
	bool Valid;
	DataType Data;
};

struct SQLDatabase
{
	SQLDatabase(bfs::path const &Path = bfs::path());
	~SQLDatabase(void);

	template <typename Signature> struct Statement;
	template <typename ...ResultTypes, typename ...ArgumentTypes>
		struct Statement<std::tuple<ResultTypes...>(ArgumentTypes...)>
	{
		Statement(sqlite3 *BaseContext, const char *Template) : Template(Template), BaseContext(BaseContext), Context(nullptr)
		{
			if (sqlite3_prepare_v2(BaseContext, Template, -1, &Context, 0) != SQLITE_OK)
				throw SystemError() << "Could not prepare query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
			assert(sizeof...(ArgumentTypes) == sqlite3_bind_parameter_count(Context));
			assert(sizeof...(ResultTypes) == sqlite3_column_count(Context));
		}

		~Statement(void)
		{
			sqlite3_finalize(Context);
		}

		void Execute(ArgumentTypes const & ...Arguments, std::function<void(ResultTypes && ...)> const &Function = std::function<void(ResultTypes & ...)>())
		{
			if (sizeof...(Arguments) > 0)
				Bind(BaseContext, Context, 1, Arguments...);
			while (true)
			{
				int Result = sqlite3_step(Context);
				if (Result == SQLITE_DONE) break;
				if (Result != SQLITE_ROW)
					throw SystemError() << "Could not execute query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Unbind<std::tuple<>, std::tuple<ResultTypes...>>(Context, 0, Function);
			}
			sqlite3_reset(Context);
		}

		Optional<std::tuple<ResultTypes ...>> GetTuple(ArgumentTypes const & ...Arguments)
		{
			bool Done = false;
			std::tuple<ResultTypes ...> Out;
			Execute(Arguments..., [&Out, &Done](ResultTypes && ...Results)
			{
				if (Done) return;
				Done = true;
				Out = std::make_tuple(std::forward<ResultTypes>(Results)...);
			});
			if (!Done) return {};
			return Out;
		}

		private:
			template <typename Enable = void, int ResultCount = sizeof...(ResultTypes)> struct GetInternal
			{
				void Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This) {}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount == 1>::type, ResultCount>
			{
				static Optional<typename std::tuple_element<0, std::tuple<ResultTypes ...>>::type>
					Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This, ArgumentTypes const & ...Arguments)
				{
					auto Out = This.GetTuple(Arguments...);
					if (!Out) return {};
					return {std::get<0>(Out.Data)};
				}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount != 1>::type, ResultCount>
			{
				static Optional<std::tuple<ResultTypes ...>>
					Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This, ArgumentTypes const & ...Arguments)
					{ return This.GetTuple(Arguments...); }
			};
		public:

		auto Get(ArgumentTypes const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		auto operator()(ArgumentTypes const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		private:
			void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int const Index) {}

			template <typename ...RemainingTypes>
				void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int const Index, std::string const &Value, RemainingTypes... RemainingArguments)
			{
				if (sqlite3_bind_text(Context, Index, Value.c_str(), Value.size(), nullptr) != SQLITE_OK)
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Bind(BaseContext, Context, Index + 1, RemainingArguments...);
			}

			template <typename IntegerType, typename... RemainingTypes, typename std::enable_if<std::is_integral<IntegerType>::value>::type* = nullptr>
				void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int const Index, IntegerType const &Value, RemainingTypes... RemainingArguments)
			{
				if (sqlite3_bind_int(Context, Index, Value) != SQLITE_OK)
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Bind(BaseContext, Context, Index + 1, RemainingArguments...);
			}

			template <typename BinaryType, typename... RemainingTypes, typename std::enable_if<!std::is_integral<BinaryType>::value>::type* = nullptr>
				void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int const Index, BinaryType const &Value, RemainingTypes... RemainingArguments)
			{
				if (sqlite3_bind_blob(Context, Index, &Value, sizeof(Value), nullptr) != SQLITE_OK)
					throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Bind(BaseContext, Context, Index + 1, RemainingArguments...);
			}

			template <typename ReadTypes, typename UnreadTypes, typename Enable = void> struct Unbind {};

			template <typename ...ReadTypes, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<std::string, RemainingTypes...>>
			{
				Unbind(sqlite3_stmt *Context, int const Index, std::function<void(ResultTypes && ...)> const &Function, ReadTypes && ...ReadData)
				{
					std::string MoreData = (char const *)sqlite3_column_text(Context, Index);
					Unbind<std::tuple<ReadTypes..., std::string>, std::tuple<RemainingTypes...>>(Context, Index + 1, Function, std::forward<ReadTypes>(ReadData)..., std::forward<std::string>(MoreData));
				}
			};

			template <typename ...ReadTypes, typename IntegerType, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<IntegerType, RemainingTypes...>, typename std::enable_if<std::is_integral<IntegerType>::value>::type>
			{
				Unbind(sqlite3_stmt *Context, int const Index, std::function<void(ResultTypes && ...)> const &Function, ReadTypes && ...ReadData)
				{
					IntegerType MoreData = sqlite3_column_int(Context, Index);
					Unbind<std::tuple<ReadTypes..., IntegerType>, std::tuple<RemainingTypes...>>(Context, Index + 1, Function, std::forward<ReadTypes>(ReadData)..., std::forward<IntegerType>(MoreData));
				}
			};

			template <typename ...ReadTypes, typename BinaryType, typename ...RemainingTypes>
				struct Unbind<std::tuple<ReadTypes...>, std::tuple<BinaryType, RemainingTypes...>, typename std::enable_if<!std::is_integral<BinaryType>::value>::type>
			{
				Unbind(sqlite3_stmt *Context, int const Index, std::function<void(ResultTypes && ...)> const &Function, ReadTypes && ...ReadData)
				{
					BinaryType MoreData;
					assert(sqlite3_column_bytes(Context, Index) == sizeof(MoreData));
					memcpy(&MoreData, sqlite3_column_blob(Context, Index), sizeof(MoreData));
					Unbind<std::tuple<ReadTypes..., BinaryType>, std::tuple<RemainingTypes...>>(Context, Index + 1, Function, std::forward<ReadTypes>(ReadData)..., std::forward<BinaryType>(MoreData));
				}
			};

			template <typename ...ReadTypes> struct Unbind<std::tuple<ReadTypes...>, std::tuple<>>
			{
				Unbind(sqlite3_stmt *Context, int const Index, std::function<void(ResultTypes && ...)> const &Function, ReadTypes && ...ReadData)
				{
					Function(std::forward<ReadTypes>(ReadData)...);
				}
			};

			const char *Template;
			sqlite3 *BaseContext;
			sqlite3_stmt *Context;
	};
	template <typename ...ArgumentTypes>
		struct Statement<void(ArgumentTypes...)> : Statement<std::tuple<>(ArgumentTypes...)>
	{
		using Statement<std::tuple<>(ArgumentTypes...)>::Statement;
	};
	template <typename ResultType, typename ...ArgumentTypes>
		struct Statement<ResultType(ArgumentTypes...)> : Statement<std::tuple<ResultType>(ArgumentTypes...)>
	{
		using Statement<std::tuple<ResultType>(ArgumentTypes...)>::Statement;
	};

	template <typename Signature> std::unique_ptr<Statement<Signature>> Prepare(char const *Template)
		{ return std::unique_ptr<Statement<Signature>>{new Statement<Signature>(Context, Template)}; }

	template <typename ...ArgumentTypes> void Execute(char const *Template, ArgumentTypes const & ...Arguments)
	{
		Statement<void(ArgumentTypes...)> Statement(Context, Template);
		Statement.Execute(Arguments...);
	}

	template <typename Signature, typename ...ArgumentTypes>
		auto Get(char const *Template, ArgumentTypes const & ...Arguments) -> decltype(Statement<Signature>(nullptr, Template).Get(Arguments...))
		{ return Statement<Signature>(Context, Template).Get(Arguments...); }

	private:
		sqlite3 *Context;
};

#endif

