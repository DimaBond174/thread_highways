#ifndef THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYS_MANAGER_H
#define THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYS_MANAGER_H

#include <thread_highways/highways/highway.h>
#include <thread_highways/tools/make_self_shared.h>
#include <thread_highways/tools/small_tools.h>

#include <algorithm> // sort
#include <mutex>
#include <vector>

namespace hi
{

/*
 * Класс может принимать в работу задачи для размещения на многопоточке
 *  + может выдавать однопоточные хайвеи для использования в однопоточке где-то ещё.
 * Создан чтобы размещать задачи на простаивающих однопоточных хайвеях.
 * Балансирует нагрузку при выдаче хайвеев.
 */
class HighWaysManager
{
public:
	struct HighWaySettings
	{
		std::chrono::milliseconds max_task_execution_time_;
		std::uint32_t mail_box_capacity_;
	};

private:
	struct HighWayHolder
	{
		HighWayHolder(std::shared_ptr<HighWay> highway)
			: highway_{std::move(highway)}
		{
		}

		std::shared_ptr<HighWay> highway_;
		std::uint32_t current_load_{0u};
	};

public:
	HighWaysManager(
		std::weak_ptr<HighWaysManager> self_weak,
		std::size_t local_workers_cnt = 1u,
		std::size_t highways_min_cnt = 1u,
		bool auto_regulation = true,
		ExceptionHandler exception_handler =
			[](const hi::Exception & ex)
		{
			throw ex;
		},
		std::string highways_manager_name = "HighWaysManager",
		std::uint32_t multi_thread_mail_box_capacity = 65000u,
		HighWaySettings highways_settings = HighWaySettings{std::chrono::milliseconds{}, 65000u})
		: self_weak_{std::move(self_weak)}
		, multi_thread_mail_box_{std::make_shared<MailBox<Runnable>>()}
		, exception_handler_{std::move(exception_handler)}
		, highways_manager_name_{std::move(highways_manager_name)}
		, highways_settings_{std::move(highways_settings)}
		, highways_min_cnt_{highways_min_cnt ? highways_min_cnt : 1u}
		, auto_regulation_{auto_regulation}
	{
		assert(multi_thread_mail_box_capacity > 0u);
		assert(highways_settings_.mail_box_capacity_ > 0u);

		multi_thread_mail_box_->set_capacity(multi_thread_mail_box_capacity);

		// Запускаю рабочие потоки которые специализируются только на многопоточке
		// (предполагается что highways_ могут быть загружены задачами более высокого приоритета,
		// и чтобы гарантировать что многопоточные задачи тоже будет обрабатываться - есть local_workers_)
		if (!local_workers_cnt)
			local_workers_cnt = 1u;
		if (highways_settings_.max_task_execution_time_ == std::chrono::milliseconds{})
		{
			start_workers_without_time_control(local_workers_cnt);
		}
		else
		{
			start_workers_with_time_control(local_workers_cnt);
		}

		// Запускаю хайвеи которые будут, в свободное от основной работы время,
		// помогать обрабатывать многопоточные задачи
		for (std::size_t i = 0u; i < highways_min_cnt_; ++i)
		{
			highways_.emplace_back(new_holder());
		}
	}

	~HighWaysManager()
	{
		destroy();
	}

	// Получить пул задач для многопоточки.
	// Например для встраивания во внешний хайвей который не управляется этим мэнеджером.
	std::shared_ptr<MailBox<Runnable>> get_multi_thread_mail_box()
	{
		return multi_thread_mail_box_;
	}

	// Пример использования get_multi_thread_mail_box()
	// Создание хайвея без управления данным менеджером.
	// Например чтобы быть уверенным что не будет отключения хайвея или конкуренции на однопоточке,
	// но при этом будет возможно заполнять свободные мощности потока
	std::shared_ptr<HighWay> create_detached_highway(
		ExceptionHandler exception_handler =
			[](const hi::Exception & ex)
		{
			throw ex;
		},
		std::string highway_name = "HighWay",
		std::chrono::milliseconds max_task_execution_time = {},
		std::uint32_t mail_box_capacity = 65000u)
	{
		return hi::make_self_shared<hi::HighWay>(
			std::move(exception_handler),
			std::move(highway_name),
			max_task_execution_time,
			mail_box_capacity,
			get_multi_thread_mail_box());
	}

	// Основной метод получение однопоточного хайвея
	// expected_load - приблизительная оценка в % планируемой загрузки чтобы сбалансировать пул хайвеев
	HighWayProxyPtr get_highway(std::uint32_t expected_load_percent)
	{
		if (auto_regulation_)
		{
			return get_highway_with_auto_regulation(expected_load_percent);
		}
		return get_highway_no_auto_regulation(expected_load_percent);
	}

	std::size_t size()
	{
		std::lock_guard lg_{mutex_};
		return running_local_workers_ + highways_.size();
	}

	// Будет пытаться добавить задачу, если ресурсов не осталось то заблокируется в ожидании
	void execute(Runnable && runnable)
	{
		execute_impl(std::move(runnable));
	}

	/**
	 * Setting a task for execution
	 *
	 * @param r - task for execution
	 * @param send_may_fail - is this task required to be completed?
	 *	(some tasks can be skipped if there is not enough RAM)
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
	template <typename R>
	void execute(R && r, const char * filename = __FILE__, const unsigned int line = __LINE__)
	{
		execute_impl(Runnable::create<R>(std::move(r), filename, line));
	}

	/**
	 * Setting a task for execution
	 *
	 * @param r - task for execution
	 * @param protector - task protection (a way to cancel task)
	 * @param send_may_fail - is this task required to be completed?
	 *	(some tasks can be skipped if there is not enough RAM)
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
	template <typename R, typename P>
	void execute(R && r, P protector, const char * filename, const unsigned int line)
	{
		execute_impl(Runnable::create<R>(std::move(r), std::move(protector), filename, line));
	}

	// Попытается добавить задачу, если ресурсов не осталось, то вернёт false
	bool try_execute(Runnable && runnable)
	{
		return try_execute_impl(std::move(runnable));
	}

	/**
	 * Setting a task for execution
	 *
	 * @param r - task for execution
	 * @param send_may_fail - is this task required to be completed?
	 *	(some tasks can be skipped if there is not enough RAM)
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
	template <typename R>
	bool try_execute(R && r, const char * filename = __FILE__, const unsigned int line = __LINE__)
	{
		return try_execute_impl(Runnable::create<R>(std::move(r), filename, line));
	}

	/**
	 * Setting a task for execution
	 *
	 * @param r - task for execution
	 * @param protector - task protection (a way to cancel task)
	 * @param send_may_fail - is this task required to be completed?
	 *	(some tasks can be skipped if there is not enough RAM)
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
	template <typename R, typename P>
	bool try_execute(R && r, P protector, const char * filename, const unsigned int line)
	{
		return try_execute_impl(Runnable::create<R>(std::move(r), std::move(protector), filename, line));
	}

private:
	void destroy()
	{
		keep_execution_ = false;
		multi_thread_mail_box_->destroy();

		std::lock_guard lg_{mutex_};
		for (auto & it : highways_)
		{
			it->highway_->destroy();
		}

		while (running_local_workers_.load(std::memory_order_acquire) > 0u)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		for (auto & it : local_workers_)
		{
			it.join();
		}
	}

	void execute_impl(Runnable && runnable)
	{
		multi_thread_mail_box_->send_may_blocked(std::move(runnable));
	}

	bool try_execute_impl(Runnable && runnable)
	{
		return multi_thread_mail_box_->send_may_fail(std::move(runnable));
	}

	std::shared_ptr<HighWayHolder> new_holder()
	{
		auto highway = hi::make_self_shared<hi::HighWay>(
			[self_weak = self_weak_, this](const hi::Exception & ex)
			{
				if (auto alive = self_weak.lock())
				{
					exception_handler_(ex);
				}
			},
			highways_manager_name_,
			highways_settings_.max_task_execution_time_,
			highways_settings_.mail_box_capacity_,
			multi_thread_mail_box_);
		return std::make_shared<HighWayHolder>(std::move(highway));
	}

	HighWayProxyPtr get_highway_with_auto_regulation(std::uint32_t expected_load)
	{
		std::lock_guard lg_{mutex_};
		std::shared_ptr<HighWayHolder> holder = highways_[highways_.size() - 1u];
		if (holder->current_load_ + expected_load > 100)
		{
			holder = new_holder();
			highways_.emplace_back(holder);
		}
		holder->current_load_ += expected_load;
		std::weak_ptr<HighWayHolder> weak_holder = holder;
		HighWayProxyPtr re = std::make_shared<HighWayProxy>(
			holder->highway_,
			[self_weak = self_weak_, weak_holder, expected_load, this]()
			{
				if (auto alive = self_weak.lock())
				{
					if (auto holder = weak_holder.lock())
					{
						std::lock_guard lg_{mutex_};
						holder->current_load_ -= expected_load;
						sort_highways();
						if (holder->current_load_ == 0u && highways_.size() > highways_min_cnt_)
						{
							auto holder = highways_[highways_.size() - 1u];
							holder->highway_->destroy();
							highways_.resize(highways_.size() - 1u);
						}
					}
				}
			});
		sort_highways();
		return re;
	}

	HighWayProxyPtr get_highway_no_auto_regulation(std::uint32_t expected_load)
	{
		std::lock_guard lg_{mutex_};
		auto holder = highways_[highways_.size() - 1u];
		holder->current_load_ += expected_load;
		std::weak_ptr<HighWayHolder> weak_holder = holder;
		HighWayProxyPtr re = std::make_shared<HighWayProxy>(
			holder->highway_,
			[self_weak = self_weak_, weak_holder, expected_load, this]()
			{
				if (auto alive = self_weak.lock())
				{
					if (auto holder = weak_holder.lock())
					{
						std::lock_guard lg_{mutex_};
						holder->current_load_ -= expected_load;
						sort_highways();
					}
				}
			});
		sort_highways();
		return re;
	}

	void sort_highways()
	{
		std::sort(
			highways_.begin(),
			highways_.end(),
			[](const std::shared_ptr<HighWayHolder> & a, const std::shared_ptr<HighWayHolder> & b)
			{
				return a->current_load_ > b->current_load_;
			});
	}

	void start_workers_without_time_control(std::uint32_t number_of_workers)
	{
		running_local_workers_ += number_of_workers;
		for (std::uint32_t i = 0; i < number_of_workers; ++i)
		{
			local_workers_.emplace_back(RAIIthread(std::thread(
				[this]
				{
					worker_loop_without_time_control();
				})));
		}
	}

	void worker_loop_without_time_control()
	{
		const auto execute_runnable = [&](Holder<Runnable> * holder)
		{
			if (!holder)
				return;
			try
			{
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(
					hi::Exception{highways_manager_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			multi_thread_mail_box_->free_holder(holder);
		};

		// main loop
		while (keep_execution_.load(std::memory_order_relaxed))
		{
			execute_runnable(multi_thread_mail_box_->pop_message());
		} // while main loop

		running_local_workers_.fetch_sub(1u, std::memory_order_release);
	} // worker_loop_without_time_control

	void start_workers_with_time_control(std::uint32_t number_of_workers)
	{
		running_local_workers_ += number_of_workers;
		for (std::uint32_t i = 0; i < number_of_workers; ++i)
		{
			local_workers_.emplace_back(RAIIthread(std::thread(
				[this]
				{
					worker_loop_with_time_control();
				})));
		}
	}

	void worker_loop_with_time_control()
	{
		Finally finally{[&]
						{
							running_local_workers_.fetch_sub(1u, std::memory_order_release);
						}};

		auto before_time = std::chrono::steady_clock::now();
		const auto execute_runnable = [&](Holder<Runnable> * holder)
		{
			if (!holder)
				return;
			before_time = std::chrono::steady_clock::now();
			try
			{
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(
					hi::Exception{highways_manager_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			before_time = after_time;
			if (time_diff > highways_settings_.max_task_execution_time_)
			{
				exception_handler_(hi::Exception{
					highways_manager_name_ + ":Runnable:stuck for ms = " + std::to_string(time_diff.count()),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line()});
			}

			multi_thread_mail_box_->free_holder(holder);
		};

		// main loop
		while (keep_execution_.load(std::memory_order_relaxed))
		{
			execute_runnable(multi_thread_mail_box_->pop_message());
		} // while main loop
	} // worker_loop_with_time_control

private:
	const std::weak_ptr<HighWaysManager> self_weak_;
	const std::shared_ptr<MailBox<Runnable>> multi_thread_mail_box_;
	const ExceptionHandler exception_handler_;
	const std::string highways_manager_name_;
	const HighWaySettings highways_settings_;
	const std::size_t highways_min_cnt_;
	// Может ли HighWaysManager самостоятельно добавлять/удалять хайвеи
	const bool auto_regulation_;

	std::recursive_mutex mutex_;
	std::vector<std::shared_ptr<HighWayHolder>> highways_;

	std::atomic<std::uint32_t> running_local_workers_{0u};
	std::vector<RAIIthread> local_workers_;

	// одноразовый рубильник
	std::atomic<bool> keep_execution_{true};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYS_MANAGER_H
