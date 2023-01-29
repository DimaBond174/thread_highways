/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_LINUX_THREAD_TOOLS_H
#define THREADS_HIGHWAYS_TOOLS_LINUX_THREAD_TOOLS_H

#include <algorithm>
#include <cstring>
#include <string>

#include <pthread.h>

namespace hi
{
namespace
{

[[maybe_unused]] void set_this_thread_name(const std::string & name)
{
	std::string shortened_name{"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
	constexpr size_t pthread_name_length_limit = 15;
	const auto length = std::min(name.size(), pthread_name_length_limit);
	if (!length)
		return;
	std::copy_n(name.begin(), length, shortened_name.begin());
	shortened_name[length] = '\0';
	pthread_setname_np(pthread_self(), shortened_name.data());
}

[[maybe_unused]] std::string get_this_thread_name()
{
    std::string re(32, '\0');
    const auto code = pthread_getname_np(pthread_self(), re.data(), re.size());
    if (0 == code)
    {
        re.resize(std::strlen(re.c_str()));
    }
	return re;
}

} // namespace
} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_LINUX_THREAD_TOOLS_H
