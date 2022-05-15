/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace hi
{

using namespace std::chrono_literals;

struct CustomFreeTimeLogic
{
	enum class HRstrategy
	{
		DecreaseWorkers,
		FreezeNumberOfWorkers,
		IncreaseWorkers
	};

	std::chrono::milliseconds operator()(
		HighWayBundle &,
		const std::uint32_t,
		const std::chrono::milliseconds work_time)
	{
		switch (hr_strategy_)
		{
		case HRstrategy::DecreaseWorkers:
			return work_time + std::chrono::milliseconds{1000000};
		case HRstrategy::FreezeNumberOfWorkers:
			return work_time;
		case HRstrategy::IncreaseWorkers:
			return {};
		}
		return {};
	}

	std::string get_code_filename()
	{
		return __FILE__;
	}

	unsigned int get_code_line()
	{
		return __LINE__;
	}

	std::atomic<HRstrategy> hr_strategy_{HRstrategy::IncreaseWorkers};
};

using highway_types =
	::testing::Types<ConcurrentHighWay<CustomFreeTimeLogic>, ConcurrentHighWayDebug<CustomFreeTimeLogic>>;

template <class T>
struct TestMultithreading : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestMultithreading, highway_types);

TYPED_TEST(TestMultithreading, IncreaseDecreaseWorkers)
{
	auto test = [&]
	{
		std::string log;
		std::mutex log_protector;

		std::set<std::thread::id> threads;
		std::mutex threads_protector;

		auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
		auto publish = [&]
		{
			auto start = std::chrono::steady_clock::now();
			while (std::chrono::steady_clock::now() - start < 300ms)
			{
				publisher->publish(0);
				std::this_thread::sleep_for(1ms);
			}
		};

		{
			RAIIdestroy highway{hi::make_self_shared<TypeParam>(
				"HighWay",
				hi::create_default_logger(
					[&](std::string msg)
					{
						std::lock_guard lg{log_protector};
						log = std::move(msg);
					}),
				10ms, // max_task_execution_time
				1ms // workers_change_period
				)};

			highway.object_->set_max_concurrent_workers(2);
			publisher->subscribe(
				[&](std::int32_t)
				{
					{
						std::lock_guard lg{threads_protector};
						threads.insert(std::this_thread::get_id());
					}
					std::this_thread::sleep_for(10ms);
				},
				highway.object_->protector_for_tests_only(),
				highway.object_->mailbox());

			// save thread id's
			publish();
			std::uint32_t last_size{0};
			{
				std::lock_guard lg{threads_protector};
				last_size = static_cast<std::uint32_t>(threads.size());
				EXPECT_GT(last_size, 1);
			}

			highway.object_->free_time_logic().hr_strategy_ = CustomFreeTimeLogic::HRstrategy::DecreaseWorkers;
			highway.object_->flush_tasks();
			// wait for decreasing
			publish();

			{
				std::lock_guard lg{threads_protector};
				threads.clear();
			}

			// save thread id's
			publish();

			{
				std::lock_guard lg{threads_protector};
				EXPECT_GT(last_size, static_cast<std::uint32_t>(threads.size()));
			}
		} // scope

		EXPECT_NE(log.find("destroyed"), std::string::npos);
	};

	for (int i = 0; i < 10; ++i)
	{
		test();
	}
}

} // namespace hi
