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

using highway_types = ::testing::Types<SerialHighWayWithScheduler<>, SingleThreadHighWayWithScheduler<>>;

template <class T>
struct TestReschedulableRunnableCallbacks : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestReschedulableRunnableCallbacks, highway_types);

TYPED_TEST(TestReschedulableRunnableCallbacks, RunAndWaitResult)
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

	highway->add_reschedulable_runnable(
		[&](ReschedulableRunnable::LaunchParameters)
		{
			promise.set_value(true);
		},
		__FILE__,
		__LINE__);
	check();

	highway->add_reschedulable_runnable(
		[&](ReschedulableRunnable::Schedule &, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			promise.set_value(true);
		},
		__FILE__,
		__LINE__);
	check();

	highway->add_reschedulable_runnable(
		[&](ReschedulableRunnable::Schedule &)
		{
			promise.set_value(true);
		},
		__FILE__,
		__LINE__);
	check();

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()(ReschedulableRunnable::LaunchParameters)
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->add_reschedulable_runnable(Fun{promise}, __FILE__, __LINE__);
		check();

		highway->add_reschedulable_runnable(std::make_unique<Fun>(promise), __FILE__, __LINE__);
		check();
	}

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()(ReschedulableRunnable::Schedule &, const std::atomic<std::uint32_t> &, const std::uint32_t)
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->add_reschedulable_runnable(Fun{promise}, __FILE__, __LINE__);
		check();

		highway->add_reschedulable_runnable(std::make_unique<Fun>(promise), __FILE__, __LINE__);
		check();
	}

	{
		struct Fun
		{
			Fun(std::reference_wrapper<std::promise<bool>> promise)
				: promise_{std::move(promise)}
			{
			}
			void operator()(ReschedulableRunnable::Schedule &)
			{
				promise_.get().set_value(true);
			}

			std::reference_wrapper<std::promise<bool>> promise_;
		};

		highway->add_reschedulable_runnable(Fun{promise}, __FILE__, __LINE__);
		check();

		highway->add_reschedulable_runnable(std::make_unique<Fun>(promise), __FILE__, __LINE__);
		check();
	}

	highway->destroy();
}

} // namespace
} // namespace hi
