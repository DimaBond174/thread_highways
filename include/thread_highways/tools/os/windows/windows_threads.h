#ifndef WINDOWS_THREADS_H
#define WINDOWS_THREADS_H

#include <Windows.h>

#include <algorithm>
#include <array>
#include <string>

namespace hi
{

namespace
{
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // must be 0x1000
	LPCSTR szName; // pointer to name (in user addr space)
	DWORD dwThreadID; // thread ID (-1=caller thread)
	DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

[[nodiscard]] std::string & thread_name_buffer()
{
	static thread_local std::string name_buffer;
	return name_buffer;
}

} // namespace

void set_this_thread_name(const std::string & name)
{
	static constexpr DWORD set_thread_name_exception_code = 0x406D1388;

	static const auto handle_set_thread_name = static_cast<PVECTORED_EXCEPTION_HANDLER>(
		[](_In_ PEXCEPTION_POINTERS info) -> LONG
		{
			if (set_thread_name_exception_code == info->ExceptionRecord->ExceptionCode)
			{
				return EXCEPTION_CONTINUE_EXECUTION;
			}
			else
			{
				return EXCEPTION_CONTINUE_SEARCH;
			}
		});

	if (const auto handler = AddVectoredExceptionHandler(1, handle_set_thread_name); handler != nullptr)
	{
		auto & buffered_name = thread_name_buffer();
		buffered_name = std::string{name.begin(), name.end()};

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = buffered_name.c_str();
		info.dwThreadID = static_cast<DWORD>(-1);
		info.dwFlags = 0;

		RaiseException(
			set_thread_name_exception_code,
			0,
			sizeof(info) / sizeof(DWORD),
			reinterpret_cast<ULONG_PTR *>(&info));

		RemoveVectoredExceptionHandler(handler);
	}
}

std::string get_this_thread_name()
{
	return thread_name_buffer();
}

} // namespace hi

#endif // WINDOWS_THREADS_H
