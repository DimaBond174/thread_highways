/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef HIGHWAYSMONITORING_H
#define HIGHWAYSMONITORING_H

#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/tools/raii_thread.h>
#include <thread_highways/tools/stack.h>

#include <chrono>
#include <condition_variable>

namespace hi
{

/*
 * Предполагается что в программе такой сервис только один,
 * и поэтому self_check() у хайвеев выполняется в одном потоке
 */
class HighWaysMonitoring
{
public:
	HighWaysMonitoring(std::chrono::milliseconds check_interval)
		: check_interval_{check_interval}
	{
		start();
	}

	~HighWaysMonitoring()
	{
		destroy();
	}

	// Если хайвей добавлен в мониторинг, то нет необходимости вызывать destroy()
	// - destroy() будет вызван у всех добавленных хайвеев при завершении потока HighWaysMonitoring::worker_thread_
	void add_for_monitoring(std::shared_ptr<IHighWay> highway)
	{
		highways_stack_.push(new Holder<std::shared_ptr<IHighWay>>{std::move(highway)});
	}

	void destroy()
	{
		global_keep_run_.store(false, std::memory_order_release);
		cv_.notify_all();
		worker_thread_.join();
	}

	void pause()
	{
		paused_.store(false, std::memory_order_release);
	}

	void resume()
	{
		paused_.store(true, std::memory_order_release);
	}

private:
	void start()
	{
		worker_thread_ = RAIIthread(std::thread(
			[this]
			{
				worker_thread_loop();
			}));
	}

	void worker_thread_loop()
	{
		std::unique_lock<std::mutex> lk(cv_m_);
		SingleThreadStack<Holder<std::shared_ptr<IHighWay>>> local_highways_stack_from;
		SingleThreadStack<Holder<std::shared_ptr<IHighWay>>> local_highways_stack_to;
		{ // finaliser scope
			RAIIfinaliser finaliser{
				[&]
				{
					// on destroy
					highways_stack_.move_to(local_highways_stack_from);
					for (auto holder = local_highways_stack_from.get_first(); holder; holder = holder->next_in_stack_)
					{
						holder->t_->destroy();
					}
					for (auto holder = local_highways_stack_to.get_first(); holder; holder = holder->next_in_stack_)
					{
						holder->t_->destroy();
					}
				}};
			do
			{
				highways_stack_.move_to(local_highways_stack_from);
				for (auto holder = local_highways_stack_from.pop(); holder; holder = local_highways_stack_from.pop())
				{
					if (holder->t_->self_check())
					{
						local_highways_stack_to.push(holder);
					}
					else
					{
						delete holder;
					}
					if (!global_keep_run_.load(std::memory_order_acquire))
					{
						return;
					}
				}

				if (!global_keep_run_.load(std::memory_order_relaxed))
				{
					break;
				}
				local_highways_stack_to.swap(local_highways_stack_from);

				do
				{
					cv_.wait_for(lk, check_interval_);
					if (!global_keep_run_.load(std::memory_order_acquire))
					{
						return;
					}
				}
				while (paused_.load(std::memory_order_acquire));
			}
			while (true); // main loop
		} // finaliser scope
	}

private:
	const std::chrono::milliseconds check_interval_;
	std::atomic_bool global_keep_run_{true};
	RAIIthread worker_thread_;
	ThreadSafeStack<Holder<std::shared_ptr<IHighWay>>> highways_stack_;

	std::condition_variable cv_;
	std::mutex cv_m_;
	std::atomic_bool paused_{false};
};

} // namespace hi

#endif // HIGHWAYSMONITORING_H
