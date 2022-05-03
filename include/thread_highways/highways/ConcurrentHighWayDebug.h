#ifndef ConcurrentHighWayDebug_H
#define ConcurrentHighWayDebug_H

#include <thread_highways/highways/FreeTimeLogic.h>
#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/raii_thread.h>

namespace hi
{

/*
	Магистраль с главным потоком и саморемонтом в случае зависания главного потока.
	Главный поток может запускать вспомогательные потоки если
	загружен работой по времени больше чем отдыхом.
*/
template <typename FreeTimeLogic = FreeTimeLogicDefaultForConcurrent>
class ConcurrentHighWayDebug : public IHighWay
{
public:
	ConcurrentHighWayDebug(
		std::shared_ptr<ConcurrentHighWayDebug> self_protector,
		std::string highway_name = "ConcurrentHighWayDebug",
		LoggerPtr logger = nullptr,
		std::chrono::milliseconds max_task_execution_time = std::chrono::milliseconds{50000},
		std::chrono::milliseconds workers_change_period = std::chrono::milliseconds{5000})
		: IHighWay{std::move(self_protector), std::move(highway_name), std::move(logger), max_task_execution_time}
		, workers_change_period_{workers_change_period}
	{
		on_create();
	}

	~ConcurrentHighWayDebug() override
	{
		destroy();
		bundle_.log("destroyed", __FILE__, __LINE__);
	}

	void set_max_concurrent_workers(std::uint32_t max_concurrent_workers)
	{
		max_concurrent_workers_ = max_concurrent_workers;
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

	bool current_execution_on_this_highway() override
	{
		/*
		 * Если во как надо, то можно при изменении набора потоков сбрасывать их Id шники в вектор,
		 * и тут сверять совпадает ли  std::this_thread::get_id()
		 * с одним из значений вектора.. Но обычно для многопоточки такое не надо т.к. потоков много и из набор
		 * меняется.
		 */
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

	void main_worker_thread_loop(const std::uint32_t your_run_id, const std::shared_ptr<IHighWay> self_protector)
	{
		const std::atomic<std::uint32_t> & global_run_id = self_protector->global_run_id();
		std::uint32_t workers_count{0};
		Workers workers;
		{ // finaliser scope
			RAIIfinaliser finaliser{[&]
									{
										bundle_.task_start_time_.store(steady_clock_now(), std::memory_order_relaxed);
										bundle_.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Stopped);
										bundle_.log("Stopped", __FILE__, __LINE__);
										stop_workers(workers);
									}};
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
					bundle_.task_start_time_.store(steady_clock_now(), std::memory_order_release);
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
						bundle_.log(
							"MailBoxMessage:exception:",
							holder->t_.get_code_filename(),
							holder->t_.get_code_line());
					}
					bundle_.log(":MailBoxMessage:stop", holder->t_.get_code_filename(), holder->t_.get_code_line());

					bundle_.mail_box_.free_holder(holder);

					if (your_run_id != global_run_id.load(std::memory_order_relaxed))
					{
						return;
					}
				} // while

				const auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - start_time);
				const auto sleep_time = free_time_logic_(bundle_, your_run_id, work_time);

				if (your_run_id != global_run_id.load(std::memory_order_acquire))
				{
					break;
				}
				bundle_.mail_box_.load_new_messages();
				bundle_.mail_box_.signal_to_work_queue_semaphore(workers_count);

				const auto sleep_finish_time = std::chrono::steady_clock::now();
				const auto last_change_time = std::chrono::duration_cast<std::chrono::milliseconds>(
					sleep_finish_time - last_time_workers_count_change);

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
		} // finaliser scope
	} // main_worker_thread_loop

	void on_stuck_go_restart(
		const std::chrono::milliseconds stuck_time,
		HighWayBundle::WhatRunningNow what_is_running_now)
	{
		const std::uint32_t max_repairs = max_repairs_.load(std::memory_order_relaxed);
		const std::uint32_t current_repairs = pending_joins_.size();
		if (bundle_.logger_)
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
			const auto start_id = holder->t_.worker_bundle_->worker_run_id_.fetch_add(1, std::memory_order_relaxed) + 1;
			holder->t_.worker_thread_ = RAIIthread(std::thread(
				[this, start_id, bundle = holder->t_.worker_bundle_]
				{
					worker_thread_loop(start_id, bundle);
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
					return;
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
	FreeTimeLogic free_time_logic_{};

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
