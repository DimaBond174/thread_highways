#ifndef ConcurrentHighWayDebug_H
#define ConcurrentHighWayDebug_H

#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/raii_thread.h>

#include <cassert>
#include <optional>

namespace hi
{

/*
	Магистраль с главным потоком и саморемонтом в случае зависания главного потока.
	Главный поток может запускать вспомогательные потоки если
	загружен работой по времени больше чем отдыхом.
*/
class ConcurrentHighWayDebug : public IHighWay
{
public:
	ConcurrentHighWayDebug(
		std::shared_ptr<ConcurrentHighWayDebug> self_protector,
		std::optional<HighWayMainLoopRunnable> main_loop_logic = std::nullopt,
		std::shared_ptr<std::atomic<std::uint32_t>> global_run_id = nullptr,
		std::string highway_name = "ConcurrentHighWayDebug",
		ErrorLoggerPtr error_logger = nullptr,
		std::chrono::milliseconds max_task_execution_time = std::chrono::milliseconds{50000},
		std::chrono::milliseconds workers_change_period = std::chrono::milliseconds{5000})
		: IHighWay{std::move(self_protector), std::move(global_run_id), std::move(highway_name), std::move(error_logger), max_task_execution_time}
		, main_loop_logic_{std::move(main_loop_logic)}
		, workers_change_period_{workers_change_period}
	{
		assert(bundle_.error_logger_);
		on_create();
	}

	~ConcurrentHighWayDebug() override
	{
		destroy();
		main_loop_logic_.reset();
	}

	void set_max_concurrent_workers(std::uint32_t max_concurrent_workers)
	{
		max_concurrent_workers_ = max_concurrent_workers;
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

	struct WorkerBundle
	{
		std::atomic<std::uint32_t> worker_run_id_{};
	};

	struct Worker
	{
		Worker()
			: worker_bundle_{std::make_shared<WorkerBundle>()}
		{
		}

		std::shared_ptr<WorkerBundle> worker_bundle_;
		RAIIthread worker_thread_;
	};

	struct Workers
	{
		SingleThreadStackWithCounter<Holder<Worker>> active_workers_stack_;
		SingleThreadStackWithCounter<Holder<Worker>> passive_workers_stack_;
	};

	void main_worker_thread_loop(const std::uint32_t your_run_id)
	{
		const std::atomic<std::uint32_t> & global_run_id = *bundle_.global_run_id_.get();
		std::uint32_t workers_count{0};
		Workers workers;
		auto last_time_workers_count_change = std::chrono::steady_clock::now();
		std::chrono::milliseconds average_work_time{0};
		std::chrono::milliseconds average_sleep_time{0};
		while (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			const auto start_time = std::chrono::steady_clock::now();
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

			const auto work_finish_time = std::chrono::steady_clock::now();

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
			bundle_.mail_box_.signal_to_work_queue_semaphore(workers_count);

			const auto sleep_finish_time = std::chrono::steady_clock::now();
			const auto last_change_time = std::chrono::duration_cast<std::chrono::milliseconds>(
				sleep_finish_time - last_time_workers_count_change);
			const auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(work_finish_time - start_time);
			const auto sleep_time =
				std::chrono::duration_cast<std::chrono::milliseconds>(sleep_finish_time - work_finish_time);
			average_work_time = (average_work_time + work_time) / 2;
			average_sleep_time = (average_sleep_time + sleep_time) / 2;
			bundle_.log(
				std::string{"average_work_time="}.append(std::to_string(average_work_time.count())
															 .append(", average_sleep_time=")
															 .append(std::to_string(average_sleep_time.count()))
															 .append(", last_change_time=")
															 .append(std::to_string(last_change_time.count()))),
				__FILE__,
				__LINE__);
			if (last_change_time > workers_change_period_)
			{
				last_time_workers_count_change = sleep_finish_time;
				if (average_work_time > average_sleep_time)
				{
					workers_count = increase_workers(workers);
				}
				else if (average_work_time < average_sleep_time)
				{
					workers_count = decrease_workers(workers);
				}
			}
		} // while
		bundle_.task_start_time_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
		bundle_.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Stopped);
		bundle_.log("Stopped", __FILE__, __LINE__);
		stop_workers(workers);
	} // main_worker_thread_loop

	void on_stuck_go_restart(
		const std::chrono::milliseconds stuck_time,
		HighWayBundle::WhatRunningNow what_is_running_now)
	{
		const std::uint32_t max_repairs = max_repairs_.load(std::memory_order_relaxed);
		const std::uint32_t current_repairs = pending_joins_.size();
		if (bundle_.error_logger_)
		{
			std::string msg("on_stuck_go_restart():");
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

	std::uint32_t increase_workers(Workers & workers)
	{
		bundle_.log("increase_workers():start", __FILE__, __LINE__);
		auto holder = workers.passive_workers_stack_.pop();
		if (holder)
		{
			holder->t_.worker_thread_.join();
		}
		else if (
			(workers.passive_workers_stack_.size() + workers.active_workers_stack_.size())
			< max_concurrent_workers_.load(std::memory_order_relaxed))
		{
			holder = new Holder<Worker>(Worker{});
		}
		if (holder)
		{
			holder->t_.worker_bundle_->worker_run_id_.fetch_add(1, std::memory_order_relaxed);
			holder->t_.worker_thread_ = RAIIthread(std::thread(
				[this,
				 your_run_id = holder->t_.worker_bundle_->worker_run_id_.load(std::memory_order_relaxed),
				 bundle = holder->t_.worker_bundle_]
				{
					worker_thread_loop(your_run_id, bundle);
				}));
			workers.active_workers_stack_.push(holder);
		}
		bundle_.log("increase_workers():stop", __FILE__, __LINE__);
		return workers.active_workers_stack_.size();
	}

	std::uint32_t decrease_workers(Workers & workers)
	{
		auto holder = workers.active_workers_stack_.pop();
		if (!holder)
		{
			return 0;
		}
		bundle_.log("decrease_workers()", __FILE__, __LINE__);
		holder->t_.worker_bundle_->worker_run_id_.fetch_add(1, std::memory_order_release);
		workers.passive_workers_stack_.push(holder);
		return workers.active_workers_stack_.size();
	}

	void stop_workers(Workers & workers)
	{
		for (auto it = workers.active_workers_stack_.get_first(); it; it = it->next_in_stack_)
		{
			it->t_.worker_bundle_->worker_run_id_.fetch_add(1);
		}

		for (auto it = workers.passive_workers_stack_.get_first(); it; it = it->next_in_stack_)
		{
			it->t_.worker_thread_.join();
		}

		for (auto it = workers.active_workers_stack_.get_first(); it; it = it->next_in_stack_)
		{
			it->t_.worker_thread_.join();
		}
	}

	void worker_thread_loop(const std::uint32_t your_run_id, const std::shared_ptr<WorkerBundle> worker_bundle)
	{
		const std::atomic<std::uint32_t> & global_run_id = worker_bundle->worker_run_id_;
		while (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			while (auto holder = bundle_.mail_box_.pop_message())
			{
				bundle_.log("MailBoxMessage:start", holder->t_.get_code_filename(), holder->t_.get_code_line());
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
				bundle_.log("MailBoxMessage:stop", holder->t_.get_code_filename(), holder->t_.get_code_line());

				bundle_.mail_box_.free_holder(holder);

				if (your_run_id != global_run_id.load(std::memory_order_acquire))
				{
					break;
				}
			} // while
			if (your_run_id == global_run_id.load(std::memory_order_relaxed))
			{
				bundle_.log("worker_wait_for_messages()", __FILE__, __LINE__);
				bundle_.mail_box_.worker_wait_for_messages();
			}
		} // while
		bundle_.log("worker stopped", __FILE__, __LINE__);
	} // worker_thread_loop

private:
	std::optional<HighWayMainLoopRunnable> main_loop_logic_;

	// как часто можно автоматически менять количество вспомогательных рабочих
	// (если ресурсов нехватка, то рабочие добавляются. Во время простоя количество рабочих сокращается)
	const std::chrono::milliseconds workers_change_period_;

	RAIIthread main_worker_thread_;
	ThreadSafeStackWithCounter<Holder<RAIIthread>> pending_joins_;

	// сколько раз хайвей имеет право себя отремонтировать
	// (если требуется регулярный ремонт, то надо что-то в коде явно менять)
	std::atomic<std::uint32_t> max_repairs_{100};

	// сколько можно добавить вспомогательных рабочих
	std::atomic<std::uint32_t> max_concurrent_workers_{2};
};

} // namespace hi

#endif // ConcurrentHighWayDebug_H
