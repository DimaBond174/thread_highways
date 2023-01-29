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

/// Milliseconds from the beginning of time to the present
[[maybe_unused]] inline std::chrono::milliseconds steady_clock_now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - std::chrono::time_point<std::chrono::steady_clock>{});
}

/**
 * Milliseconds from the beginning of time to the specified time
 * @param time
 * @return milliseconds
 */
[[maybe_unused]] inline std::chrono::milliseconds steady_clock_from(
	std::chrono::time_point<std::chrono::steady_clock> time)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		time - std::chrono::time_point<std::chrono::steady_clock>{});
}

/**
 * Container to call method destroy() on the object when exiting the scope
 * Used to automatically stop highways.
 * Example:
 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/connections_notifier/src/connections_notifier.cpp#L8
 * Unit test: https://github.com/DimaBond174/thread_highways/blob/main/tests/highways/destroy/src/test_destroy.cpp#L196
 */
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

    T& operator->()
    {
        return object_;
    }

	T object_;
};

/**
 * Container to execute functor when exiting the scope.
 * Used to perform finishing tasks when exiting the Highway Reactor.
 * Example:
 * https://github.com/DimaBond174/thread_highways/blob/main/include/thread_highways/highways/ConcurrentHighWay.h
 */
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

} // namespace hi

#endif // SMALL_TOOLS_H
