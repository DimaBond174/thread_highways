#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>

using namespace std::chrono_literals;

TEST(ScheduleTest, BasicSchedule)
{
	auto highway = hi::make_self_shared<hi::SerialHighWayWithScheduler>(
		std::nullopt,
		nullptr,
		"SerialHighWay:serial_schedule",
		nullptr,
		10ms,
		10ms);

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

TEST(ScheduleTest, ScheduleWithHighWaysMonitoring)
{
	auto highway = hi::make_self_shared<hi::SerialHighWayWithScheduler>(
		std::nullopt,
		nullptr,
		"SerialHighWay:serial_schedule",
		nullptr,
		10ms,
		10ms);

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