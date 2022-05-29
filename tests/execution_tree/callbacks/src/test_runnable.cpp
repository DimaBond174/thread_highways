/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

namespace hi
{
namespace
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
struct TestRunnableCallbacks : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestRunnableCallbacks, highway_types);

TYPED_TEST(TestRunnableCallbacks, RunAndWaitResult)
{
	const auto highway = hi::make_self_shared<TypeParam>();
	std::promise<bool> promise;
	auto future = promise.get_future();

	const auto check = [&]
	{
		EXPECT_TRUE(future.get());
		promise = {};
		future = promise.get_future();
	};

	highway->post(
		[&]()
		{
			promise.set_value(true);
		});
	check();

	highway->post(
		[&](Runnable::LaunchParameters)
		{
			promise.set_value(true);
		});
	check();

	highway->post(
		[&](const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			promise.set_value(true);
		});
	check();

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()()
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->post(Fun{promise});
		check();

		highway->post(std::make_unique<Fun>(promise));
		check();
	}

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()(Runnable::LaunchParameters)
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->post(Fun{promise});
		check();

		highway->post(std::make_unique<Fun>(promise));
		check();
	}

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()(const std::atomic<std::uint32_t> &, const std::uint32_t)
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->post(Fun{promise});
		check();

		highway->post(std::make_unique<Fun>(promise));
		check();
	}

	highway->destroy();
}

} // namespace
} // namespace hi
