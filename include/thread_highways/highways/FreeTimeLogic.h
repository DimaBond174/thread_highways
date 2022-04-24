#ifndef FREETIMELOGIC_H
#define FREETIMELOGIC_H

#include <thread_highways/highways/IHighWay.h>
#include <thread_highways/tools/small_tools.h>

#include <atomic>
#include <cstdint>

/*
	Набор плагинов для инжектирования в главный цикл хайвеев.
	Используется для:
	- Изменения принципа ожидания новых сообщений
	- Подсчёта времени бездействия
	- Добавления возможности поставить хайвей на паузу
	- Выполнения некой пользовательской логики
*/

namespace hi
{

struct FreeTimeLogicDefault
{
	void operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t /* your_run_id */,
		const std::chrono::nanoseconds max_wait_time = std::chrono::nanoseconds{100000000})
	{
		highway_bundle.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Sleep, std::memory_order_release);
		highway_bundle.mail_box_.wait_for_new_messages(max_wait_time);
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}
};

class FreeTimeLogicWithPauses
{
public:
	void operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t your_run_id,
		const std::chrono::nanoseconds max_wait_time = std::chrono::nanoseconds{100000000})
	{
		highway_bundle.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Sleep, std::memory_order_release);
		highway_bundle.mail_box_.wait_for_new_messages(max_wait_time);
		const std::atomic<std::uint32_t> & global_run_id = highway_bundle.global_run_id_;
		while (your_run_id == global_run_id.load(std::memory_order_acquire) && paused_.load(std::memory_order_acquire))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
		}
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}

	void pause()
	{
		paused_.store(true, std::memory_order_release);
	}

	void resume()
	{
		paused_.store(false, std::memory_order_release);
	}

private:
	std::atomic_bool paused_{false};
};

class FreeTimeLogicCustomExample
{
public:
	void operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t your_run_id,
		[[maybe_unused]] const std::chrono::nanoseconds max_wait_time = std::chrono::nanoseconds{100000000})
	{
		const std::atomic<std::uint32_t> & global_run_id = highway_bundle.global_run_id_;

		if (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			highway_bundle.task_start_time_.store(
				steady_clock_from(std::chrono::steady_clock::now()),
				std::memory_order_relaxed);
			highway_bundle.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::FreeTimeCustomLogic,
				std::memory_order_release);

			// put your custom logic here:
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
		}
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}

	void you_custom_method()
	{
		++i;
	}

private:
	std::atomic<std::uint32_t> i{0};
};

struct FreeTimeLogicDefaultForConcurrent
{
	std::chrono::milliseconds operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t /* your_run_id */,
		const std::chrono::milliseconds /* work_time */)
	{
		const auto start = std::chrono::steady_clock::now();
		highway_bundle.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Sleep, std::memory_order_release);
		highway_bundle.mail_box_.wait_for_new_messages();
		const auto finish = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}
};

class FreeTimeLogicWithPausesForConcurrent
{
public:
	std::chrono::milliseconds operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t your_run_id,
		const std::chrono::milliseconds /* work_time */)
	{
		const auto start = std::chrono::steady_clock::now();
		highway_bundle.what_is_running_now_.store(HighWayBundle::WhatRunningNow::Sleep, std::memory_order_release);
		highway_bundle.mail_box_.wait_for_new_messages();
		const auto finish = std::chrono::steady_clock::now();
		const std::atomic<std::uint32_t> & global_run_id = highway_bundle.global_run_id_;
		while (your_run_id == global_run_id.load(std::memory_order_acquire) && paused_.load(std::memory_order_acquire))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
		}
		return std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}

	void pause()
	{
		paused_.store(true, std::memory_order_release);
	}

	void resume()
	{
		paused_.store(false, std::memory_order_release);
	}

private:
	std::atomic_bool paused_{false};
};

class FreeTimeLogicWithPausesForConcurrentCustomExample
{
public:
	std::chrono::milliseconds operator()(
		HighWayBundle & highway_bundle,
		const std::uint32_t your_run_id,
		const std::chrono::milliseconds work_time)
	{
		const std::atomic<std::uint32_t> & global_run_id = highway_bundle.global_run_id_;
		enum class HRstrategy
		{
			DecreaseWorkers,
			FreezeNumberOfWorkers,
			IncreaseWorkers
		};
		HRstrategy hr_strategy = HRstrategy::FreezeNumberOfWorkers;
		// while (your_run_id == global_run_id.load(std::memory_order_acquire))
		if (your_run_id == global_run_id.load(std::memory_order_acquire))
		{
			highway_bundle.task_start_time_.store(
				steady_clock_from(std::chrono::steady_clock::now()),
				std::memory_order_relaxed);
			highway_bundle.what_is_running_now_.store(
				HighWayBundle::WhatRunningNow::FreeTimeCustomLogic,
				std::memory_order_release);

			// put your custom logic here:
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
			hr_strategy = HRstrategy::IncreaseWorkers;
		}
		switch (hr_strategy)
		{
		case HRstrategy::DecreaseWorkers:
			return work_time + std::chrono::milliseconds{1};
		case HRstrategy::FreezeNumberOfWorkers:
			return work_time;
		case HRstrategy::IncreaseWorkers:
			return {};
		}
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}

	void you_custom_method()
	{
		++i;
	}

private:
	std::atomic<std::uint32_t> i{0};
};

} // namespace hi

#endif // FREETIMELOGIC_H
