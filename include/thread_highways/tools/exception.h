/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_EXCEPTION_H
#define THREADS_HIGHWAYS_TOOLS_EXCEPTION_H

#include <thread_highways/tools/call_stack.h>

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sstream>

namespace hi
{

template <typename T>
struct DsonRecord
{
	T data;
	std::uint32_t key;
};

class Exception : public std::exception
{
public:
	Exception() noexcept = default;
	Exception(const char * filename, unsigned int line) noexcept
		: call_stack_{get_call_stack()}
		, filename_{filename}
		, line_{line}
	{
	}

	Exception(std::string what, const char * filename, unsigned int line) noexcept
		: call_stack_{get_call_stack()}
		, what_{std::move(what)}
		, filename_{filename}
		, line_{line}
	{
	}

	template <typename... AddInfo>
	Exception(std::string what, const char * filename, unsigned int line, AddInfo &&... infos)
		: call_stack_{get_call_stack()}
		, what_{std::move(what)}
		, filename_{filename}
		, line_{line}
	{
		add_infoN(std::forward<AddInfo>(infos)...);
	}

	template <typename T, typename... AddInfo>
	void add_infoN(T && info, AddInfo... infos)
	{
		add_info1(std::forward<T>(info));
		if constexpr (sizeof...(infos) > 0)
		{
			add_infoN(std::forward<AddInfo>(infos)...);
		}
		// todo dson_.add(info.key, std::move(info.data))
	}

	void add_info1(std::exception_ptr e)
	{
		if (!e)
			return;
		try
		{
			std::rethrow_exception(e);
		}
		catch (const std::runtime_error & e)
		{
			what_.append("std::runtime_error: ").append(e.what());
		}
		catch (const std::exception & e)
		{
			what_.append("std::exception: ").append(e.what());
		}
		catch (...)
		{
			what_.append("unknown exception..");
		}
	}

	const CallStack & call_stack() const noexcept
	{
		return call_stack_;
	}

	const char * file_name() const noexcept
	{
		return filename_;
	}

	unsigned int file_line() const noexcept
	{
		return line_;
	}

	const char * what() const noexcept override
	{
		return what_.c_str();
	}

	const std::string & what_as_string() const noexcept
	{
		return what_;
	}

	std::string all_info_as_astring() const noexcept
	{
		std::stringstream ss;
		ss << *this;
		return ss.str();
	}

private:
	template <typename Stream>
	friend Stream & operator<<(Stream & os, const Exception & e)
	{
		os << e.filename_ << ':' << e.line_ << "\n:" << e.what_as_string();
		os << e.call_stack_;
		return os;
	}

private:
	CallStack call_stack_;
	std::string what_;
	const char * filename_;
	unsigned int line_;
};

using ExceptionHandler = std::function<void(const hi::Exception &)>;

} // namespace hi

#define HI_EXPR_AS_STRING(x) #x
#define HI_ASSERT(expr) \
	if (!(expr)) \
	throw hi::Exception(HI_EXPR_AS_STRING(expr), __FILE__, __LINE__)
#define HI_ASSERT_INFO(expr, str) \
	if (!(expr)) \
	throw hi::Exception(std::string{}.append(HI_EXPR_AS_STRING(expr)).append(" : ").append((str)), __FILE__, __LINE__)

#endif // THREADS_HIGHWAYS_TOOLS_EXCEPTION_H
