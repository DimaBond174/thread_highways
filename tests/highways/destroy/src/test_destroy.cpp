/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>

namespace hi
{

using highway_types = ::testing::Types<HighWay, MultiThreadedTaskProcessingPlant>;

template <class T>
struct TestHighwayDestroy : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestHighwayDestroy, highway_types);

struct OnDestroyWatcher
{
	OnDestroyWatcher(std::atomic<int> & counter)
		: counter_{counter}
	{
		++counter_;
	}

	~OnDestroyWatcher()
	{
		--counter_;
	}
	OnDestroyWatcher(const OnDestroyWatcher &) = delete;
	OnDestroyWatcher & operator=(const OnDestroyWatcher &) = delete;
	OnDestroyWatcher(OnDestroyWatcher &&) = delete;
	OnDestroyWatcher & operator=(OnDestroyWatcher &&) = delete;

	std::atomic<int> & counter_;
};

TYPED_TEST(TestHighwayDestroy, DirectDestroy)
{
	auto highway = hi::make_self_shared<TypeParam>();
	std::atomic<int> counter{0};

	highway->execute(
		[&]()
		{
			static thread_local OnDestroyWatcher watcher{counter};
		});

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(counter, 1);

	highway->destroy();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(counter, 0);
}

TYPED_TEST(TestHighwayDestroy, DestroyWithRAIIdestroy)
{
	std::atomic<int> counter{0};
	{
		RAIIdestroy guard{hi::make_self_shared<TypeParam>()};
		guard.object_->execute(
			[&]()
			{
				static thread_local OnDestroyWatcher watcher{counter};
			});
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		EXPECT_EQ(counter, 1);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(counter, 0);
}

} // namespace hi
