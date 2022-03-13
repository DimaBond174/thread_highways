#ifndef SerialHighWayDebug_H
#define SerialHighWayDebug_H

#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/raii_thread.h>

#include <optional>

namespace hi
{

/*
	Магистраль с одним потоком и саморемонтом в случае зависания потока.
*/
class SerialHighWayDebug : public IHighWay
{
public:
	SerialHighWayDebug(
		std::shared_ptr<SerialHighWayDebug> self_protector,
		std::optional<HighWayMainLoopRunnable> main_loop_logic = std::nullopt,
		std::shared_ptr<std::atomic<std::uint32_t>> global_run_id = nullptr,
		std::string highway_name = "SerialHighWayDebug",
		ErrorLoggerPtr error_logger = nullptr,
		std::chrono::milliseconds max_task_execution_time = std::chrono::milliseconds{50000})
		: IHighWay{std::move(self_protector), std::move(global_run_id), std::move(highway_name), std::move(error_logger), max_task_execution_time}
		, main_loop_logic_{std::move(main_loop_logic)}
	{
		on_create();
	}

	~SerialHighWayDebug() override
	{
		destroy();
		main_loop_logic_.reset();
	}

	bool current_execution_on_this_highway()
	{
		return std::this_thread::get_id() == main_worker_thread_id_.load(std::memory_order_acquire);
	}

	void set_max_repairs(std::uint32_t max_repairs)
	{
		max_repairs_ = max_repairs;
	}

	void destroy() override
	{
		++(*bundle_.global_run_id_);

		bundle_.mail_box_.destroy();

		main_worker_thread_.join();
		for (auto it = pending_joins_.access_stack(); it; it = it->next_in_stack_)
		{
			it->t_.join();
		}

		self_protector_.reset();
	}

	bool self_check() override
	{
		const auto what_is_running_now = bundle_.what_is_running_now_.load(std::memory_order_acquire);
		if (HighWayBundle::WhatRunningNow::Sleep == what_is_running_now)
		{
			return true;
		}

		const auto this_time = std::chrono::steady_clock::now();
		const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			this_time - bundle_.task_start_time_.load(std::memory_order_acquire));
		if (time_diff > bundle_.max_task_execution_time_)
		{
			on_stuck_go_restart(time_diff, what_is_running_now);
		}
		return true;
	}

private:
	void recreate_main_worker()
	{
		main_worker_thread_ = RAIIthread(std::thread(
			[this]
			{
				main_worker_thread_loop(bundle_.global_run_id_->load(std::memory_order_relaxed));
			}));
	}

	void on_create()
	{
		recreate_main_worker();
	}

	void main_worker_thread_loop(const std::uint32_t your_run_id)
	{
		const std::atomic<std::uint32_t> & global_run_id = *bundle_.global_run_id_.get();
		if (your_run_id == global_run_id)
		{
			main_worker_thread_id_ = std::this_thread::get_id();
		}

		while (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			bundle_.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::MailBoxMessage,
				std::memory_order_relaxed);
			while (auto holder = bundle_.mail_box_.pop_message())
			{
				bundle_.log("MailBoxMessage:start", holder->t_.get_code_filename(), holder->t_.get_code_line());
				bundle_.task_start_time_.store(std::chrono::steady_clock::now(), std::memory_order_release);
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
				bundle_.log(":MailBoxMessage:stop", holder->t_.get_code_filename(), holder->t_.get_code_line());

				bundle_.mail_box_.free_holder(holder);

				if (your_run_id != global_run_id.load(std::memory_order_relaxed))
				{
					break;
				}
			} // while

			if (your_run_id != global_run_id.load(std::memory_order_relaxed))
			{
				break;
			}

			if (main_loop_logic_)
			{
				bundle_.task_start_time_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
				bundle_.what_is_running_now_.store(
					HighWayBundle::WhatRunningNow::MainLoopLogic,
					std::memory_order_release);

				bundle_.log(
					"MainLoopLogic:start",
					main_loop_logic_->get_code_filename(),
					main_loop_logic_->get_code_line());
				main_loop_logic_->run(bundle_, your_run_id);
				bundle_.log(
					"MainLoopLogic:stop",
					main_loop_logic_->get_code_filename(),
					main_loop_logic_->get_code_line());
			}
			else
			{
				bundle_.log("Sleep", __FILE__, __LINE__);
				bundle_.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Sleep, std::memory_order_release);
				bundle_.mail_box_.wait_for_new_messages();
			}
			if (your_run_id != global_run_id.load(std::memory_order_acquire))
			{
				break;
			}
			bundle_.mail_box_.load_new_messages();
		} // while

		bundle_.task_start_time_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
		bundle_.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Stopped);
		bundle_.log("Stopped", __FILE__, __LINE__);
	} // main_worker_thread_loop

	void on_stuck_go_restart(
		const std::chrono::milliseconds stuck_time,
		HighWayBundle::WhatRunningNow what_is_running_now)
	{
		const std::uint32_t max_repairs = max_repairs_.load(std::memory_order_relaxed);
		const std::uint32_t current_repairs = pending_joins_.size();
		if (bundle_.error_logger_)
		{
			std::string msg{bundle_.highway_name_};
			if (current_repairs >= max_repairs)
			{
				msg.append(":can't going to restart because pending_joins_ >= max_repairs_(")
					.append(std::to_string(max_repairs))
					.append("), ");
			}
			else
			{
				msg.append(":going to restart because ");
			}
			msg.append(" task stuck for ").append(std::to_string(stuck_time.count()));
			switch (what_is_running_now)
			{
			case HighWayBundle::WhatRunningNow::Sleep:
				{
					break;
				}
			case HighWayBundle::WhatRunningNow::OtherLogic:
				{
					msg.append(" milliseconds on WhatRunningNow::OtherLogic");
					break;
				}
			case HighWayBundle::WhatRunningNow::MailBoxMessage:
				{
					msg.append(" milliseconds on WhatRunningNow::MailBoxMessage");
					break;
				}
			case HighWayBundle::WhatRunningNow::MainLoopLogic:
				{
					msg.append(" milliseconds on WhatRunningNow::MainLoopLogic")
						.append(main_loop_logic_->get_code_filename())
						.append(", at: ")
						.append(std::to_string(main_loop_logic_->get_code_line()));
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

		if (current_repairs >= max_repairs || !bundle_.is_stuck())
		{
			return;
		}
		++(*bundle_.global_run_id_);
		pending_joins_.push(new Holder<RAIIthread>{std::move(main_worker_thread_)});
		recreate_main_worker();
	}

private:
	std::optional<HighWayMainLoopRunnable> main_loop_logic_;
	RAIIthread main_worker_thread_;
	std::atomic<std::thread::id> main_worker_thread_id_;
	ThreadSafeStackWithCounter<Holder<RAIIthread>> pending_joins_;

	// сколько раз хайвей имеет право себя отремонтировать
	// (если требуется регулярный ремонт, то надо что-то в коде явно менять)
	std::atomic<std::uint32_t> max_repairs_{100};
};

} // namespace hi

#endif // SerialHighWayDebug_H
