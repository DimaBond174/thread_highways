/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>

using namespace std::chrono_literals;

TEST(TestSchedule, BasicSchedule)
{
	auto highway =
		hi::make_self_shared<hi::SerialHighWayWithScheduler<>>("SerialHighWay:serial_schedule", nullptr, 10ms, 10ms);

	std::promise<int> promise;
	auto future = promise.get_future();
	highway->add_reschedulable_runnable(hi::ReschedulableRunnable::create(
		[&, i = 0](
			hi::ReschedulableRunnable::Schedule & schedule,
			const std::atomic<std::uint32_t> &,
			const std::uint32_t) mutable
		{
			++i;
			if (i < 10)
			{
				schedule.schedule_launch_in(100ms);
			}
			else
			{
				promise.set_value(i);
			}
		},
		__FILE__,
		__LINE__));

	EXPECT_EQ(future.get(), 10);
	highway->destroy();
}

TEST(TestSchedule, ScheduleWithHighWaysMonitoring)
{
	auto highway =
		hi::make_self_shared<hi::SerialHighWayWithScheduler<>>("SerialHighWay:serial_schedule", nullptr, 10ms, 10ms);

	hi::HighWaysMonitoring monitoring{100ms};
	monitoring.add_for_monitoring(highway);

	std::promise<int> promise;
	auto future = promise.get_future();
	highway->add_reschedulable_runnable(hi::ReschedulableRunnable::create(
		[&, i = 0](
			hi::ReschedulableRunnable::Schedule & schedule,
			const std::atomic<std::uint32_t> &,
			const std::uint32_t) mutable
		{
			++i;
			std::this_thread::sleep_for(100ms);
			if (i < 10)
			{
				schedule.schedule_launch_in(100ms);
			}
			else
			{
				promise.set_value(i);
			}
		},
		__FILE__,
		__LINE__));

	EXPECT_EQ(future.get(), 10);
}

TEST(TestSchedule, CheckScheduleStartTimes)
{
	const std::chrono::milliseconds timer_executes_each_ms{100ms};
	const std::chrono::milliseconds timer_epsilon{timer_executes_each_ms * 2};
	auto highway = hi::make_self_shared<hi::SerialHighWayWithScheduler<>>(
		"SerialHighWay:serial_schedule",
		nullptr,
		10ms,
		timer_executes_each_ms);

	const std::uint32_t scheduled_tasks{10};
	hi::Semaphore tasks_done_semaphore;

	for (uint32_t i = scheduled_tasks; i; --i)
	{
		highway->add_reschedulable_runnable(hi::ReschedulableRunnable::create(
			[&,
			 launch_frequency = timer_executes_each_ms * i,
			 starts = 0,
			 expected_start_time = std::chrono::steady_clock::now()](
				hi::ReschedulableRunnable::Schedule & schedule,
				const std::atomic<std::uint32_t> &,
				const std::uint32_t) mutable
			{
				const auto now = std::chrono::steady_clock::now();
				EXPECT_TRUE(expected_start_time <= now);
				EXPECT_TRUE(now <= expected_start_time + timer_epsilon);
				++starts;
				if (starts < 100)
				{
					schedule.schedule_launch_in(launch_frequency);
					expected_start_time = std::chrono::steady_clock::now() + launch_frequency;
				}
				else
				{
					tasks_done_semaphore.signal();
				}
			},
			__FILE__,
			__LINE__));
	}

	for (std::uint32_t i = scheduled_tasks; i; --i)
	{
		tasks_done_semaphore.wait();
	}
	highway->destroy();
}

TEST(TestSchedule, ServiceWithLaunchParameters)
{
	auto highway = hi::make_self_shared<hi::SingleThreadHighWayWithScheduler<>>(
		"SingleThreadHighWayWithScheduler:test schedule",
		nullptr,
		10ms,
		10ms);

	std::promise<bool> promise;
	auto future = promise.get_future();

	std::atomic_bool long_running_algorithm_was_aborted{false};

	struct Service
	{
		Service(std::promise<bool> & promise, std::atomic_bool & long_running_algorithm_was_aborted)
			: promise_{promise}
			, long_running_algorithm_was_aborted_{long_running_algorithm_was_aborted}
		{
		}

		void operator()(hi::ReschedulableRunnable::LaunchParameters params)
		{
			hi::RAIIfinaliser finaliser{[&]
										{
											// Schedule next start
											params.schedule_.get().schedule_launch_in(100ms);
										}};
			++starts_;
			if (starts_ < 2)
			{
				return;
			}

			// Long running algorithm
			const auto start_time = std::chrono::steady_clock::now();
			promise_.get().set_value(true);
			for (auto cur_time = std::chrono::steady_clock::now(); (cur_time - start_time) < 5s;
				 cur_time = std::chrono::steady_clock::now())
			{
				std::this_thread::sleep_for(100ms);
				if ((params.global_run_id_.get() != params.your_run_id_))
				{
					long_running_algorithm_was_aborted_.get().store(true);
					break;
				}
			}
		}

		const std::reference_wrapper<std::promise<bool>> promise_;
		const std::reference_wrapper<std::atomic_bool> long_running_algorithm_was_aborted_;
		std::uint32_t starts_{0};
	};

	highway->add_reschedulable_runnable(Service{promise, long_running_algorithm_was_aborted}, __FILE__, __LINE__);

	EXPECT_TRUE(future.get());
	highway->destroy();
	EXPECT_TRUE(long_running_algorithm_was_aborted);
}

TEST(TestSchedule, ServiceWithLaunchParametersAndDestroyByProtector)
{
	auto highway = hi::make_self_shared<hi::SingleThreadHighWayWithScheduler<>>(
		"SingleThreadHighWayWithScheduler:test schedule",
		nullptr,
		10ms,
		10ms);

	std::promise<bool> promise_will_scheduled;
	auto future_will_scheduled = promise_will_scheduled.get_future();

	std::promise<bool> promise_will_destroyed_by_protector;
	auto future_will_destroyed_by_protector = promise_will_destroyed_by_protector.get_future();

	struct Service
	{
		Service(std::promise<bool> & promise_will_scheduled, std::promise<bool> & promise_will_destroyed_by_protector)
			: promise_will_scheduled_{promise_will_scheduled}
			, promise_will_destroyed_by_protector_{promise_will_destroyed_by_protector}
		{
		}
		~Service()
		{
			promise_will_destroyed_by_protector_.get().set_value(true);
		}

		void operator()(hi::ReschedulableRunnable::LaunchParameters params)
		{
			++starts_;
			if (starts_ == 2)
			{
				promise_will_scheduled_.get().set_value(true);
			}
			params.schedule_.get().schedule_launch_in(100ms);
		}

		const std::reference_wrapper<std::promise<bool>> promise_will_scheduled_;
		const std::reference_wrapper<std::promise<bool>> promise_will_destroyed_by_protector_;
		std::uint32_t starts_{0};
	};

	auto protector = std::make_shared<bool>();
	highway->add_reschedulable_runnable(
		std::make_unique<Service>(promise_will_scheduled, promise_will_destroyed_by_protector),
		std::weak_ptr(protector),
		__FILE__,
		__LINE__);

	EXPECT_TRUE(future_will_scheduled.get());
	protector.reset();
	EXPECT_TRUE(future_will_destroyed_by_protector.get());
	highway->destroy();
}
