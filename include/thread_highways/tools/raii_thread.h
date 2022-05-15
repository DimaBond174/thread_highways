/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef RAIIthread_H
#define RAIIthread_H

#include <mutex>
#include <thread>

namespace hi
{

class RAIIthread
{
public:
	RAIIthread() = default;

	explicit RAIIthread(std::thread move_thread)
		: thread_(std::move(move_thread))
	{
		if (!thread_.joinable())
		{
			throw std::logic_error("RAIIthread can't move joined thread..");
		}
		joinable_ = true;
	}

	~RAIIthread()
	{
		join();
	}

	void join()
	{
		std::lock_guard lg{mutex_};
		if (joinable_ && thread_.joinable())
		{
			thread_.join();
		}
		joinable_ = false;
	}

	RAIIthread(RAIIthread const &) = delete;
	RAIIthread(RAIIthread && other)
		: thread_(other.move_thread())
		, joinable_(thread_.joinable())
	{
	}

	RAIIthread & operator=(RAIIthread const &) = delete;
	RAIIthread & operator=(RAIIthread && other)
	{
		if (this == &other)
			return *this;
		thread_ = std::move(other.thread_);
		joinable_ = other.joinable_;
		other.joinable_ = false;
		return *this;
	}

private:
	std::thread move_thread()
	{
		std::lock_guard lg{mutex_};
		if (joinable_)
		{
			joinable_ = false;
			return std::move(thread_);
		}
		return std::thread();
	}

private:
	std::mutex mutex_;
	std::thread thread_;
	bool joinable_{false};
};

} // namespace hi

#endif // RAIIthread_H
