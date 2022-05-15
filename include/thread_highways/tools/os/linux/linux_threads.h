/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef LINUX_THREADS_H
#define LINUX_THREADS_H

#include <algorithm>
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
	std::copy_n(name.begin(), std::min(name.size(), pthread_name_length_limit), shortened_name.begin());
	pthread_setname_np(pthread_self(), shortened_name.data());
}

[[maybe_unused]] std::string get_this_thread_name()
{
	std::string re{"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
	pthread_getname_np(pthread_self(), re.data(), re.size());
	return re;
}

} // namespace
} // namespace hi

#endif // LINUX_THREADS_H
