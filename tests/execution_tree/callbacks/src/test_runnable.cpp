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

struct TestData
{
	const std::string_view description_;
	std::function<Runnable(std::reference_wrapper<std::promise<bool>>)> creator_;
};

TestData test_data[] = {
	TestData{
		"AllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise)
		{
			return Runnable::create(
				[promise](const std::atomic<bool> &)
				{
					promise.get().set_value(true);
				},
				__FILE__,
				__LINE__);
		}},
	TestData{
		"NoParameters",
		[](std::reference_wrapper<std::promise<bool>> promise)
		{
			return Runnable::create(
				[promise]()
				{
					promise.get().set_value(true);
				},
				__FILE__,
				__LINE__);
		}},
	TestData{
		"FunctorAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise)
		{
			struct Logic
			{
				Logic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(const std::atomic<bool> &)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_unique<Logic>(promise);
			return Runnable::create(std::move(logic), __FILE__, __LINE__);
		}},
	TestData{
		"FunctorNoParameters",
		[](std::reference_wrapper<std::promise<bool>> promise)
		{
			struct Logic
			{
				Logic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()()
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_unique<Logic>(promise);
			return Runnable::create(std::move(logic), __FILE__, __LINE__);
		}},
};

struct RunnableCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	RunnableCallbacksTestInstance,
	RunnableCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(RunnableCallbacksTest, RunAndWaitResultDirect)
{
	const auto highway = hi::make_self_shared<HighWay>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const TestData & test_params = GetParam();
	highway->execute(test_params.creator_(promise));
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
