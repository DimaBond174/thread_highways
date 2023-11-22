/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

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

struct HighWayTester
{
	void test(std::function<void(void)> runnable, ExceptionHandler exception_handler)
	{
		const auto highway = hi::make_self_shared<hi::HighWay>(
			std::move(exception_handler),
			"HighWayTester",
			std::chrono::milliseconds{10});
		highway->execute(std::move(runnable));
		std::this_thread::sleep_for(std::chrono::milliseconds{100});
		highway->destroy();
	}
};

struct MultiThreadedTaskProcessingPlantTester
{
	void test(std::function<void(void)> runnable, ExceptionHandler exception_handler)
	{
		const auto highway = hi::make_self_shared<hi::MultiThreadedTaskProcessingPlant>(
			2,
			std::move(exception_handler),
			"MultiThreadedTaskProcessingPlantTester",
			std::chrono::milliseconds{10});
		highway->execute(std::move(runnable));
		std::this_thread::sleep_for(std::chrono::milliseconds{100});
		highway->destroy();
	}
};

using highway_types = ::testing::Types<HighWayTester, MultiThreadedTaskProcessingPlantTester>;

template <class T>
struct TestHighwayMonitoring : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestHighwayMonitoring, highway_types);

TYPED_TEST(TestHighwayMonitoring, MonitorHungs)
{
	std::atomic<bool> was{false};
	TypeParam test;
	test.test(
		[]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
		},
		[&](const hi::Exception &)
		{
			was = true;
		});
	EXPECT_FALSE(was);

	test.test(
		[]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
		},
		[&](const hi::Exception &)
		{
			was = true;
		});
	EXPECT_TRUE(was);
}

TYPED_TEST(TestHighwayMonitoring, MonitorException)
{
	std::atomic<bool> was{false};
	TypeParam test;
	test.test(
		[]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
		},
		[&](const hi::Exception &)
		{
			was = true;
		});
	EXPECT_FALSE(was);

	test.test(
		[]
		{
			throw std::runtime_error("Oh, runtime_error error");
		},
		[&](const hi::Exception &)
		{
			was = true;
		});
	EXPECT_TRUE(was);
}

} // namespace hi
