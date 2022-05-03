#ifndef SerialHighWay_H
#define SerialHighWay_H

#include <thread_highways/highways/FreeTimeLogic.h>
#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/raii_thread.h>

namespace hi
{

/*
	Магистраль с последовательным выполнением задач.
	Гарантирует что задачи будут стартовать одна за другой,
	но не гарантирует однопоточность так как в случае саморемонта
	зависшего потока будет запущен новый поток
	 (в то время как старый может ещё работать).
*/
template <typename FreeTimeLogic = FreeTimeLogicDefault>
class SerialHighWay : public IHighWay
{
public:
	SerialHighWay(
		std::shared_ptr<SerialHighWay> self_protector,
		std::string highway_name = "SerialHighWay",
		LoggerPtr logger = nullptr,
		std::chrono::milliseconds max_task_execution_time = std::chrono::milliseconds{50000})
		: IHighWay{std::move(self_protector), std::move(highway_name), std::move(logger), max_task_execution_time}
	{
		on_create();
	}

	~SerialHighWay() override
	{
		destroy();
		bundle_.log("destroyed", __FILE__, __LINE__);
	}

	bool current_execution_on_this_highway() override
	{
		return std::this_thread::get_id() == main_worker_thread_id_.load(std::memory_order_acquire);
	}

	void set_max_repairs(std::uint32_t max_repairs)
	{
		max_repairs_ = max_repairs;
	}

	bool is_single_threaded() const noexcept override
	{
		return false;
	}

	void destroy() override
	{
		++bundle_.global_run_id_;

		bundle_.mail_box_.destroy();

		main_worker_thread_.join();
		for (auto it = pending_joins_.access_stack(); it; it = it->next_in_stack_)
		{
			it->t_.join();
		}
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
			on_stuck_go_restart(stuck_duration, what_is_running_now);
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
		while (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			bundle_.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::MailBoxMessage,
				std::memory_order_relaxed);
			while (auto holder = bundle_.mail_box_.pop_message())
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
					bundle_
						.log("MailBoxMessage:exception:", holder->t_.get_code_filename(), holder->t_.get_code_line());
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

				if (your_run_id != global_run_id.load(std::memory_order_relaxed))
				{
					return;
				}
			} // while

			free_time_logic_(bundle_, your_run_id);

			if (your_run_id != global_run_id.load(std::memory_order_acquire))
			{
				break;
			}
			bundle_.mail_box_.load_new_messages();
		} // while
	} // main_worker_thread_loop

	void on_stuck_go_restart(
		const std::chrono::milliseconds stuck_time,
		HighWayBundle::WhatRunningNow what_is_running_now)
	{
		const std::uint32_t max_repairs = max_repairs_.load(std::memory_order_relaxed);
		const std::uint32_t current_repairs = pending_joins_.size();
		if (bundle_.logger_)
		{
			std::string msg;
			if (current_repairs >= max_repairs)
			{
				msg.append("can't going to restart because pending_joins_ >= max_repairs_(")
					.append(std::to_string(max_repairs))
					.append("), ");
			}
			else
			{
				msg.append("going to restart because ");
			}
			msg.append(" task stuck for ").append(std::to_string(stuck_time.count()));
			switch (what_is_running_now)
			{
			case HighWayBundle::WhatRunningNow::Sleep:
				{
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
			default:
				break;
			}

			bundle_.log(std::move(msg), __FILE__, __LINE__);
		}

		if (current_repairs >= max_repairs || !bundle_.is_stuck())
		{
			return;
		}
		++bundle_.global_run_id_;
		pending_joins_.push(new Holder<RAIIthread>{std::move(main_worker_thread_)});
		recreate_main_worker();
	}

private:
	FreeTimeLogic free_time_logic_{};
	RAIIthread main_worker_thread_;
	std::atomic<std::thread::id> main_worker_thread_id_;
	ThreadSafeStackWithCounter<Holder<RAIIthread>> pending_joins_;

	// сколько раз хайвей имеет право себя отремонтировать
	// (если требуется регулярный ремонт, то надо что-то в коде явно менять)
	std::atomic<std::uint32_t> max_repairs_{100};
};

} // namespace hi

#endif // SerialHighWay_H
