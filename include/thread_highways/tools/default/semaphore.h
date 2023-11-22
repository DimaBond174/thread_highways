/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_DEFAULT_SEMAPHORE_H
#define THREADS_HIGHWAYS_TOOLS_DEFAULT_SEMAPHORE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace hi
{

class Semaphore
{
public:
	Semaphore()
	{
	}

	void destroy()
	{
		keep_run_.store(false, std::memory_order_release);
		signal_to_all();
	}

	bool wait()
	{
		return wait_for(std::chrono::nanoseconds{100000000});
	}

	bool wait_for(std::chrono::nanoseconds ns)
	{
		if (!keep_run_.load(std::memory_order_acquire))
		{
			return false;
		}
		std::unique_lock<std::mutex> lk(mutex_);
		if (cv_.wait_for(
				lk,
				ns,
				[this](void) -> bool
				{
					return sem_val_ > 0;
				}))
		{
			--sem_val_;
			return true;
		}
		return false;
	}

	// Если нужен true|false семафор - то не постим если уже запостили
	void signal_keep_one()
	{
		if (!keep_run_.load(std::memory_order_acquire))
		{
			return;
		}
		{
			std::lock_guard<std::mutex> lg(mutex_);
			if (sem_val_ < 2)
			{
				sem_val_ = 2;
			}
		}
		cv_.notify_all();
	}

	void signal()
	{
		if (keep_run_.load(std::memory_order_acquire))
		{
			sem_post();
		}
	}

	void signal(std::uint32_t count)
	{
		{
			std::lock_guard<std::mutex> lg(mutex_);
			sem_val_ = static_cast<std::int32_t>(count);
		}
		cv_.notify_all();
	}

	void signal_to_all()
	{
		{
			std::lock_guard<std::mutex> lg(mutex_);
			sem_val_ = 1;
		}
		cv_.notify_all();
	}

private:
	void sem_post()
	{
		bool notify{false};
		{
			std::lock_guard<std::mutex> lg(mutex_);
			++sem_val_;
			notify = sem_val_ > 0;
		}
		if (notify)
		{
			cv_.notify_all();
		}
	}

private:
	std::mutex mutex_;
	std::condition_variable cv_;
	volatile std::int32_t sem_val_{0};
	std::atomic_bool keep_run_{true};
	Semaphore(const Semaphore & other) = delete;
	Semaphore & operator=(const Semaphore & other) = delete;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_DEFAULT_SEMAPHORE_H
