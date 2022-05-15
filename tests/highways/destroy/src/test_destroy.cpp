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
#include <future>
#include <memory>
#include <mutex>

namespace hi
{

using highway_types = ::testing::Types<
	SerialHighWay<>,
	SerialHighWayDebug<>,
	SerialHighWayWithScheduler<>,
	SingleThreadHighWay<>,
	SingleThreadHighWayWithScheduler<>,
	ConcurrentHighWay<>,
	ConcurrentHighWayDebug<>>;

template <class T>
struct TestHighwayDestroy : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestHighwayDestroy, highway_types);

enum State
{
	Nothing,
	Executed,
	Destroyed
};

struct OnDestroyWatcher
{
	OnDestroyWatcher(std::atomic<State> & watcher)
		: watcher_{watcher}
	{
	}
	~OnDestroyWatcher()
	{
		auto tst_val = State::Nothing;
		watcher_.compare_exchange_strong(tst_val, State::Destroyed);
	}
	std::atomic<State> & watcher_;
};

TYPED_TEST(TestHighwayDestroy, DirectDestroy)
{
	std::string log;
	std::mutex log_protector;

	{
		auto highway = hi::make_self_shared<TypeParam>(
			"HighWay",
			hi::create_default_logger(
				[&](std::string msg)
				{
					std::lock_guard lg{log_protector};
					log = std::move(msg);
				}));

		std::atomic<State> watcher{State::Nothing};

		highway->mailbox()->send_may_blocked(hi::Runnable::create(
			[watcher = std::make_shared<OnDestroyWatcher>(
				 watcher)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				watcher->watcher_ = State::Executed;
			},
			__FILE__,
			__LINE__));

		EXPECT_NE(watcher, State::Destroyed);

		highway->flush_tasks();
		highway->destroy();

		EXPECT_EQ(watcher, State::Executed);
	} // scope

	EXPECT_NE(log.find("destroyed"), std::string::npos);
}

TYPED_TEST(TestHighwayDestroy, DestroyedByHighWaysMonitoring)
{
	std::string log;
	std::mutex log_protector;

	{
		auto highway = hi::make_self_shared<TypeParam>(
			"HighWay",
			hi::create_default_logger(
				[&](std::string msg)
				{
					std::lock_guard lg{log_protector};
					log = std::move(msg);
				}));

		hi::HighWaysMonitoring monitoring{std::chrono::milliseconds{100}};
		monitoring.add_for_monitoring(highway);

		std::atomic<State> watcher{State::Nothing};

		highway->mailbox()->send_may_blocked(hi::Runnable::create(
			[watcher = std::make_shared<OnDestroyWatcher>(watcher)]() mutable
			{
				watcher->watcher_ = State::Executed;
			},
			__FILE__,
			__LINE__));

		EXPECT_NE(watcher, State::Destroyed);

		highway->flush_tasks();

		EXPECT_EQ(watcher, State::Executed);

		monitoring.destroy();
	} // scope

	EXPECT_NE(log.find("destroyed"), std::string::npos);
}

TYPED_TEST(TestHighwayDestroy, DestroyLongRunningTask)
{
	auto test = []
	{
		std::string log;
		std::mutex log_protector;

		{
			auto highway = hi::make_self_shared<TypeParam>(
				"HighWay",
				hi::create_default_logger(
					[&](std::string msg)
					{
						std::lock_guard lg{log_protector};
						log = std::move(msg);
					}));

			hi::HighWaysMonitoring monitoring{std::chrono::milliseconds{10}};
			monitoring.add_for_monitoring(highway);

			std::promise<bool> result_promise;
			auto result_future = result_promise.get_future();

			highway->mailbox()->send_may_blocked(hi::Runnable::create(
				[result_promise = std::move(result_promise)](
					const std::atomic<std::uint32_t> & global_run_id,
					const std::uint32_t your_run_id) mutable
				{
					result_promise.set_value(true);
					while (your_run_id == global_run_id.load(std::memory_order_acquire))
					{
						std::this_thread::sleep_for(std::chrono::milliseconds{10});
					}
				},
				__FILE__,
				__LINE__));

			EXPECT_EQ(true, result_future.get());
		} // scope

		EXPECT_NE(log.find("destroyed"), std::string::npos);
	};

	for (int i = 0; i < 100; ++i)
	{
		test();
	}
}

TYPED_TEST(TestHighwayDestroy, DestroyWithRAIIdestroy)
{
	auto test = []
	{
		std::string log;
		std::mutex log_protector;

		{
			RAIIdestroy guard{hi::make_self_shared<TypeParam>(
				"HighWay",
				hi::create_default_logger(
					[&](std::string msg)
					{
						std::lock_guard lg{log_protector};
						log = std::move(msg);
					}))};

			std::promise<bool> result_promise;
			auto result_future = result_promise.get_future();

			guard.object_->mailbox()->send_may_blocked(hi::Runnable::create(
				[result_promise = std::move(result_promise)](
					const std::atomic<std::uint32_t> & global_run_id,
					const std::uint32_t your_run_id) mutable
				{
					result_promise.set_value(true);
					while (your_run_id == global_run_id.load(std::memory_order_acquire))
					{
						std::this_thread::sleep_for(std::chrono::milliseconds{10});
					}
				},
				__FILE__,
				__LINE__));

			EXPECT_EQ(true, result_future.get());
		} // scope

		EXPECT_NE(log.find("destroyed"), std::string::npos);
	};

	for (int i = 0; i < 100; ++i)
	{
		test();
	}
}

} // namespace hi
