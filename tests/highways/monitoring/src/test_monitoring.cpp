#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <type_traits>

namespace hi
{

using highway_types = ::testing::Types<
	SerialHighWay<>,
	SerialHighWayDebug<>,
	SerialHighWayWithScheduler<>,
	// can't self repair SingleThreadHighWay<>,
	// can't self repair SingleThreadHighWayWithScheduler<>,
	ConcurrentHighWay<>,
	ConcurrentHighWayDebug<>>;

template <class T>
struct TestHighwayMonitoring : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestHighwayMonitoring, highway_types);

/*
 Тест про ремонт зависших потоков:
 устанавливается что все задачи должны успевать отработать за std::chrono::milliseconds{10},
 однако на выполнение ставится задача std::this_thread::sleep_for(std::chrono::milliseconds{30});

 Система мониторинга должна перезапустить хайвей в обход зависшей задачи.
*/
TYPED_TEST(TestHighwayMonitoring, RepairHungs)
{
	auto test = []
	{
		std::promise<bool> promise_highway_will_repaired;
		auto future_highway_will_repaired = promise_highway_will_repaired.get_future();
		std::set<std::thread::id> unique_ids;
		std::mutex unique_ids_protector;

		{
			auto highway = hi::make_self_shared<TypeParam>(
				"HighWay",
				hi::create_default_logger(
					[](std::string)
					{
						// std::cout << msg << std::endl;
					}),
				std::chrono::milliseconds{10});

			if constexpr (
				std::is_same_v<
					TypeParam,
					hi::ConcurrentHighWay<>> || std::is_same_v<TypeParam, hi::ConcurrentHighWayDebug<>>)
			{
				highway->set_max_concurrent_workers(0);
			}

			hi::HighWaysMonitoring monitoring{std::chrono::milliseconds{10}};
			monitoring.add_for_monitoring(highway);

			auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
			publisher->subscribe(
				[&](std::int32_t, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds{30});
					std::lock_guard lg{unique_ids_protector};
					unique_ids.emplace(std::this_thread::get_id());
					if (unique_ids.size() > 1)
					{
						promise_highway_will_repaired.set_value(true);
					}
				},
				highway->protector_for_tests_only(),
				highway->mailbox());
			//			hi::subscribe(
			//				publisher->subscribe_channel(),
			//				highway,
			//				[&](std::int32_t, const std::atomic<std::uint32_t> &, const std::uint32_t)
			//				{
			//					std::this_thread::sleep_for(std::chrono::milliseconds{30});
			//					std::lock_guard lg{unique_ids_protector};
			//					unique_ids.emplace(std::this_thread::get_id());
			//					if (unique_ids.size() > 1)
			//					{
			//						promise_highway_will_repaired.set_value(true);
			//					}
			//				});

			std::future_status status;
			do
			{
				publisher->publish(0);
				status = future_highway_will_repaired.wait_for(std::chrono::milliseconds{10});
			}
			while (status != std::future_status::ready);

		} // scope

		EXPECT_EQ(true, future_highway_will_repaired.get());
		EXPECT_TRUE(unique_ids.size() > 1);
	};

	for (int i = 0; i < 10; ++i)
	{
		test();
	}
}

/*
	Проверяем что новые задачи не запускаются на зависших старых потоках.
*/
TYPED_TEST(TestHighwayMonitoring, MustBeSerial)
{
	auto test = []
	{
		std::promise<bool> test_done;
		auto future_test_done = test_done.get_future();
		std::vector<std::thread::id> thread_ids;
		std::mutex thread_ids_mutex;

		std::atomic<uint32_t> tasks_executed{0};
		const uint32_t max_tasks{10};

		{
			auto highway = hi::make_self_shared<TypeParam>(
				"HighWay",
				hi::create_default_logger(
					[](std::string)
					{
						// std::cout << msg << std::endl;
					}),
				std::chrono::milliseconds{10});

			if constexpr (
				std::is_same_v<
					TypeParam,
					hi::ConcurrentHighWay<>> || std::is_same_v<TypeParam, hi::ConcurrentHighWayDebug<>>)
			{
				highway->set_max_concurrent_workers(0);
			}

			hi::HighWaysMonitoring monitoring{std::chrono::milliseconds{10}};
			monitoring.add_for_monitoring(highway);

			auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
			publisher->subscribe(
				[&](std::int32_t, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					{
						const auto my_thread_id = std::this_thread::get_id();
						std::lock_guard lg{thread_ids_mutex};
						if (thread_ids.size() > 0 && thread_ids[thread_ids.size() - 1] != my_thread_id)
						{
							for (auto it : thread_ids)
							{
								EXPECT_NE(my_thread_id, it);
							}
							thread_ids.emplace_back(my_thread_id);
						}
						else if (thread_ids.size() == 0)
						{
							thread_ids.emplace_back(my_thread_id);
						}
					}

					std::this_thread::sleep_for(std::chrono::milliseconds{100});
					++tasks_executed;
					if (tasks_executed == max_tasks)
					{
						test_done.set_value(true);
					}
				},
				highway->protector_for_tests_only(),
				highway->mailbox());
			//			hi::subscribe(
			//				publisher->subscribe_channel(),
			//				highway,
			//				[&](std::int32_t, const std::atomic<std::uint32_t> &, const std::uint32_t)
			//				{
			//					{
			//						const auto my_thread_id = std::this_thread::get_id();
			//						std::lock_guard lg{thread_ids_mutex};
			//						if (thread_ids.size() > 0 && thread_ids[thread_ids.size() - 1] != my_thread_id)
			//						{
			//							for (auto it : thread_ids)
			//							{
			//								EXPECT_NE(my_thread_id, it);
			//							}
			//							thread_ids.emplace_back(my_thread_id);
			//						}
			//						else if (thread_ids.size() == 0)
			//						{
			//							thread_ids.emplace_back(my_thread_id);
			//						}
			//					}

			//					std::this_thread::sleep_for(std::chrono::milliseconds{100});
			//					++tasks_executed;
			//					if (tasks_executed == max_tasks)
			//					{
			//						test_done.set_value(true);
			//					}
			//				});

			std::future_status status;
			do
			{
				publisher->publish(0);
				status = future_test_done.wait_for(std::chrono::milliseconds{10});
			}
			while (status != std::future_status::ready);

		} // scope
	};

	for (int i = 0; i < 10; ++i)
	{
		test();
	}
}

} // namespace hi
