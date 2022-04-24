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
	const std::chrono::milliseconds timer_executes_each_ms{1ms};
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