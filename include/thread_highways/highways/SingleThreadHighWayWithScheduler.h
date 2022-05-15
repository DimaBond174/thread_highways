/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef SingleThreadHighWayWithScheduler_H
#define SingleThreadHighWayWithScheduler_H

#include <thread_highways/execution_tree/ReschedulableRunnable.h>
#include <thread_highways/highways/FreeTimeLogic.h>
#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/raii_thread.h>

namespace hi
{

/*
		Магистраль с одним потоком.
		Не умеет саморемонтироваться.
		На этой магистрали можно разместить задачи который будут выполняться
		по расписанию.
*/
template <typename FreeTimeLogic = FreeTimeLogicDefault>
class SingleThreadHighWayWithScheduler : public IHighWay
{
public:
	SingleThreadHighWayWithScheduler(
		std::shared_ptr<SingleThreadHighWayWithScheduler> self_protector,
		std::string highway_name = "SingleThreadHighWayWithScheduler",
		LoggerPtr logger = nullptr,
		std::chrono::milliseconds max_task_execution_time = std::chrono::milliseconds{50000},
		std::chrono::nanoseconds timer_executes_each_ns = std::chrono::nanoseconds{1000000000})
		: IHighWay{std::move(self_protector), std::move(highway_name), std::move(logger), max_task_execution_time}
		, timer_executes_each_ns_{timer_executes_each_ns}
	{
		on_create();
	}

	~SingleThreadHighWayWithScheduler() override
	{
		destroy();
		bundle_.log("destroyed", __FILE__, __LINE__);
	}

	void add_reschedulable_runnable(ReschedulableRunnable && runnable)
	{
		schedule_stack_.push(new hi::Holder<ReschedulableRunnable>(std::move(runnable)));
	}

	template <typename R>
	void add_reschedulable_runnable(R && r, std::string filename, const unsigned int line)
	{
		add_reschedulable_runnable(hi::ReschedulableRunnable::create(std::move(r), std::move(filename), line));
	}

	template <typename R, typename P>
	void add_reschedulable_runnable(R && r, P protector, std::string filename, const unsigned int line)
	{
		add_reschedulable_runnable(
			hi::ReschedulableRunnable::create(std::move(r), std::move(protector), std::move(filename), line));
	}

	bool current_execution_on_this_highway() override
	{
		return std::this_thread::get_id() == main_worker_thread_id_.load(std::memory_order_acquire);
	}

	bool is_single_threaded() const noexcept override
	{
		return true;
	}

	void destroy() override
	{
		++bundle_.global_run_id_;
		bundle_.mail_box_.destroy();

		main_worker_thread_.join();
	}

	bool self_check() override
	{
		const auto what_is_running_now = bundle_.what_is_running_now_.load(std::memory_order_acquire);
		if (HighWayBundle::WhatRunningNow::Sleep == what_is_running_now)
		{
			return true;
		}

		if (const auto stuck_duration = bundle_.stuck_duration(); stuck_duration > bundle_.max_task_execution_time_)
		{
			on_stuck(stuck_duration, what_is_running_now);
		}
		return true;
	}

	FreeTimeLogic & free_time_logic()
	{
		return free_time_logic_;
	}

private:
	void recreate_main_worker()
	{
		const auto start_id = bundle_.global_run_id_.load(std::memory_order_acquire);
		main_worker_thread_ = RAIIthread(std::thread(
			[this, start_id, self_protector = self_weak_.lock()]
			{
				main_worker_thread_loop(start_id, self_protector);
			}));
	}

	void on_create()
	{
		recreate_main_worker();
	}

	void main_worker_thread_loop(const std::uint32_t your_run_id, const std::shared_ptr<IHighWay> self_protector)
	{
		const std::atomic<std::uint32_t> & global_run_id = self_protector->global_run_id();
		RAIIfinaliser finaliser{[&]
								{
									bundle_.task_start_time_.store(steady_clock_now(), std::memory_order_relaxed);
									bundle_.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Stopped);
								}};
		if (your_run_id != global_run_id)
		{
			return;
		}
		main_worker_thread_id_ = std::this_thread::get_id();

		const auto max_task_execution_time = bundle_.max_task_execution_time_;

		const auto execute_runnable = [&](Holder<Runnable> * holder)
		{
			const auto before_time = std::chrono::steady_clock::now();
			bundle_.task_start_time_.store(steady_clock_from(before_time), std::memory_order_release);
			try
			{
				holder->t_.run(global_run_id, your_run_id);
			}
			catch (const std::exception & e)
			{
				bundle_.log(
					std::string{"MailBoxMessage:exception:"}.append(e.what()),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line());
			}
			catch (...)
			{
				bundle_.log("MailBoxMessage:exception:", holder->t_.get_code_filename(), holder->t_.get_code_line());
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			if (time_diff > max_task_execution_time)
			{
				bundle_.log(
					std::string{":MailBoxMessage:stuck for ms = "}.append(std::to_string(time_diff.count())),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line());
			}

			bundle_.mail_box_.free_holder(holder);
		};

		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
		const auto execute_reschedulable_runnable = [&](Holder<ReschedulableRunnable> * holder)
		{
			const auto before_time = std::chrono::steady_clock::now();
			bundle_.task_start_time_.store(steady_clock_from(before_time), std::memory_order_release);
			try
			{
				holder->t_.schedule().rechedule_ = false;
				holder->t_.run(global_run_id, your_run_id);
			}
			catch (const std::exception & e)
			{
				bundle_.log(
					std::string{"ReschedulableRunnable:exception:"}.append(e.what()),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line());
			}
			catch (...)
			{
				bundle_.log(
					"ReschedulableRunnable:exception:",
					holder->t_.get_code_filename(),
					holder->t_.get_code_line());
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			if (time_diff > max_task_execution_time)
			{
				bundle_.log(
					std::string{":ReschedulableRunnable:stuck for ms = "}.append(std::to_string(time_diff.count())),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line());
			}

			if (holder->t_.schedule().rechedule_)
			{
				schedule_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		};

		while (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			bundle_.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::ReschedulableRunnable,
				std::memory_order_relaxed);
			while (auto holder = schedule_stack_.pop())
			{
				if (std::chrono::steady_clock::now() >= holder->t_.schedule().next_execution_time_)
				{
					execute_reschedulable_runnable(holder);
				}
				else
				{
					schedule_stack.push(holder);
				}
				if (your_run_id != global_run_id.load(std::memory_order_relaxed))
				{
					break; // must schedule_stack_.fetch_from()
				}
			} // while schedule_stack_

			schedule_stack_.fetch_from(schedule_stack);

			if (your_run_id != global_run_id.load(std::memory_order_relaxed))
			{
				return;
			}

			bundle_.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::MailBoxMessage,
				std::memory_order_relaxed);
			while (auto holder = bundle_.mail_box_.pop_message())
			{
				execute_runnable(holder);
				if (your_run_id != global_run_id.load(std::memory_order_relaxed))
				{
					return;
				}
			}

			free_time_logic_(bundle_, your_run_id, timer_executes_each_ns_);

			if (your_run_id != global_run_id.load(std::memory_order_acquire))
			{
				break;
			}
			bundle_.mail_box_.load_new_messages();
		} // while
	} // main_worker_thread_loop

	void on_stuck(const std::chrono::milliseconds stuck_time, HighWayBundle::WhatRunningNow what_is_running_now)
	{
		if (bundle_.logger_)
		{
			std::string msg{"task stuck for "};
			msg.append(std::to_string(stuck_time.count()));
			switch (what_is_running_now)
			{
			case HighWayBundle::WhatRunningNow::Sleep:
				{
					break;
				}
			case HighWayBundle::WhatRunningNow::ReschedulableRunnable:
				{
					msg.append(" milliseconds on WhatRunningNow::ReschedulableRunnable");
					break;
				}
			case HighWayBundle::WhatRunningNow::MailBoxMessage:
				{
					msg.append(" milliseconds on WhatRunningNow::MailBoxMessage");
					break;
				}
			case HighWayBundle::WhatRunningNow::FreeTimeCustomLogic:
				{
					msg.append(" milliseconds on WhatRunningNow::MainLoopLogic")
						.append(free_time_logic_.get_code_filename())
						.append(", at: ")
						.append(std::to_string(free_time_logic_.get_code_line()));
					break;
				}
			case HighWayBundle::WhatRunningNow::Stopped:
				{
					msg.append(" milliseconds on WhatRunningNow::Stopped");
					break;
				}
			}

			bundle_.log(std::move(msg), __FILE__, __LINE__);
		}
	}

private:
	FreeTimeLogic free_time_logic_{};
	// Дискретизация таймера
	const std::chrono::nanoseconds timer_executes_each_ns_;

	RAIIthread main_worker_thread_;
	std::atomic<std::thread::id> main_worker_thread_id_;

	// Запланированные задачи
	ThreadSafeStack<Holder<ReschedulableRunnable>> schedule_stack_;
};

} // namespace hi

#endif // SingleThreadHighWayWithScheduler_H
