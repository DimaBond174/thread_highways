#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <iostream>

namespace hi
{

namespace
{
class FreeTimeLogicWithPausesLocal
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
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
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

class FreeTimeLogicWithPausesForConcurrentLocal
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
		if (paused_)
		{
			highway_bundle.log("FreeTimeLogic before pause", __FILE__, __LINE__);
			while (your_run_id == global_run_id.load(std::memory_order_acquire)
				   && paused_.load(std::memory_order_acquire))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds{10});
			}
			highway_bundle.log("FreeTimeLogic after pause", __FILE__, __LINE__);
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

} // namespace

using highway_types = ::testing::Types<
	SerialHighWay<FreeTimeLogicWithPausesLocal>,
	SerialHighWayDebug<FreeTimeLogicWithPausesLocal>,
	SerialHighWayWithScheduler<FreeTimeLogicWithPausesLocal>,
	SingleThreadHighWay<FreeTimeLogicWithPausesLocal>,
	SingleThreadHighWayWithScheduler<FreeTimeLogicWithPausesLocal>,
	ConcurrentHighWay<FreeTimeLogicWithPausesForConcurrentLocal>,
	ConcurrentHighWayDebug<FreeTimeLogicWithPausesForConcurrentLocal>>;

template <class T>
struct TestHighwayPauseLifecycle : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestHighwayPauseLifecycle, highway_types);

/*
 Тест про возможность поставить потоки на паузу:
 Например в Android есть понятие  lifecycle и приложение должно уметь вставать на паузу.
 Большинство приложений либо продолжают пытаться работать, либо выключаются совсем.
 Правильным поведением было бы временно приостановить работы (например пока пользователь
 отвечает на телефонный звонок).
 Проверяем что хайвеи так могут.
*/
TYPED_TEST(TestHighwayPauseLifecycle, ThreadsOnPauseAndMonitoringToo)
{
	auto test = []
	{
		std::atomic_bool threads_online{false};

		{
			hi::SharedLogger shared_logger{hi::create_default_logger(
				[]([[maybe_unused]] const std::string & msg)
				{
					// std::cout << msg << std::endl;
				})};
			auto highway = hi::make_self_shared<TypeParam>("HighWay", *shared_logger, std::chrono::milliseconds{10});

			hi::HighWaysMonitoring monitoring{std::chrono::milliseconds{10}};
			monitoring.add_for_monitoring(highway);

			auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();

			hi::subscribe(
				*publisher->subscribe_channel(),
				[&](std::int32_t, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					threads_online = true;
				},
				highway->protector_for_tests_only(),
				highway->mailbox());

			publisher->publish(0); // 1 normal publish
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
			EXPECT_EQ(true, threads_online);

			monitoring.pause();
			highway->free_time_logic().pause();
			publisher->publish(0); // 2 trigger semaphore - should enter in FreeTimeLogic
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
			threads_online = false;
			shared_logger.log("paused");

			publisher->publish(0); // 3 must wait until exit FreeTimeLogic
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
			EXPECT_EQ(false, threads_online);

			highway->free_time_logic().resume();
			std::this_thread::sleep_for(std::chrono::milliseconds{30});
			monitoring.resume();
			shared_logger.log("resumed");

			EXPECT_EQ(true, threads_online);

		} // scope
	};

	for (int i = 0; i < 10; ++i)
	{
		test();
	}
} // ThreadsOnPauseAndMonitoringToo

} // namespace hi
