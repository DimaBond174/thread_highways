/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_WINDOWS_FD_WRITE_READ_H
#define THREADS_HIGHWAYS_TOOLS_WINDOWS_FD_WRITE_READ_H

#include <WinSock2.h>

namespace hi
{
namespace
{
// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/read?view=msvc-170
inline std::int32_t read_from_fd(std::int32_t fd, void * buf, std::int32_t buf_size)
{
	// todo обработать кейс когда int64_t больше чем может read
	const auto re = ::read(fd, buf, static_cast<size_t>(buf_size));
	if (re < 0)
	{
		const auto err = errno;
		if (EAGAIN == err || EWOULDBLOCK == err)
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
	return re;
}

} // namespace
} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_WINDOWS_FD_WRITE_READ_H
