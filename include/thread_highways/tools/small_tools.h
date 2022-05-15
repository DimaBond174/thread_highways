/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef SMALL_TOOLS_H
#define SMALL_TOOLS_H

#include <chrono>
#include <memory>

namespace hi
{
namespace
{

[[maybe_unused]] std::chrono::milliseconds steady_clock_now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - std::chrono::time_point<std::chrono::steady_clock>{});
}

[[maybe_unused]] std::chrono::milliseconds steady_clock_from(std::chrono::time_point<std::chrono::steady_clock> time)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		time - std::chrono::time_point<std::chrono::steady_clock>{});
}

template <typename T>
struct RAIIdestroy
{
	RAIIdestroy(T object)
		: object_{std::move(object)}
	{
	}

	~RAIIdestroy()
	{
		object_->destroy();
	}

	T object_;
};

template <typename T>
struct RAIIfinaliser
{
	RAIIfinaliser(T runnable)
		: runnable_{std::move(runnable)}
	{
	}

	~RAIIfinaliser()
	{
		runnable_();
	}

	T runnable_;
};

} // namespace
} // namespace hi

#endif // SMALL_TOOLS_H
