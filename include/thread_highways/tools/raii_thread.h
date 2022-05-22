/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef RAIIthread_H
#define RAIIthread_H

#include <memory>
#include <mutex>
#include <thread>

namespace hi
{

/**
 * A more convenient container for std::thread,
 * making it less likely that something might change between calls thread_.joinable() and thread_.join()
 * Usage example:
 * https://github.com/DimaBond174/thread_highways/blob/main/include/thread_highways/highways/ConcurrentHighWay.h
 */
class RAIIthread
{
public:
	RAIIthread() = default;

	explicit RAIIthread(std::thread && move_thread)
		: bundle_(std::make_unique<Bundle>(std::move(move_thread)))
	{
	}

	~RAIIthread()
	{
		join();
	}

	/**
	 * @brief join
	 * Attaches a thread and removes the object
	 */
	void join()
	{
		std::lock_guard lg{mutex_};
		if (bundle_)
		{
			bundle_->join();
			bundle_.reset();
		}
	}

	RAIIthread(RAIIthread const &) = delete;
	RAIIthread(RAIIthread && other)
		: bundle_(other.move_bundle())
	{
	}

	RAIIthread & operator=(RAIIthread const &) = delete;
	RAIIthread & operator=(RAIIthread && other)
	{
		if (this == &other)
			return *this;
		join();
		{
			std::lock_guard lg{mutex_};
			bundle_ = other.move_bundle();
		}
		return *this;
	}

private:
	class Bundle
	{
	public:
		Bundle();
		Bundle(std::thread && thread)
			: thread_{std::move(thread)}
			, joinable_{thread_.joinable()}
		{
		}
		Bundle(Bundle &&) = delete;
		Bundle & operator=(Bundle &&) = delete;
		Bundle(const Bundle &) = delete;
		Bundle & operator=(const Bundle &) = delete;
		~Bundle()
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

	private:
		std::mutex mutex_;
		std::thread thread_;
		bool joinable_{false};
	};

	std::unique_ptr<Bundle> move_bundle()
	{
		std::lock_guard lg{mutex_};
		return std::move(bundle_);
	}

private:
	std::mutex mutex_;
	std::unique_ptr<Bundle> bundle_{};
};

} // namespace hi

#endif // RAIIthread_H
