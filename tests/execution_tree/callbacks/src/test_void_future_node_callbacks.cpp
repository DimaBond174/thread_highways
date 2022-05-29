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

using namespace std::string_view_literals;

struct Parameter
{
	int parameter_;
};

struct Result
{
	int result_;
};

struct TestData
{
	const std::string_view description_;
	std::function<std::shared_ptr<hi::VoidFutureNode<Result>>(std::reference_wrapper<std::promise<bool>>, IHighWayPtr)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"LaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise](hi::VoidFutureNodeLogic<Result>::LaunchParameters)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"AllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise](IPublisher<Result> &, INode &, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"ParametersWithoutRunControl",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise](IPublisher<Result> &, INode &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutPublishProgress",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise](IPublisher<Result> &, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"ResultOnly",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise](IPublisher<Result> &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::VoidFutureNode<Result>::create(
				[promise]()
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorLaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(hi::VoidFutureNodeLogic<Result>::LaunchParameters)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(IPublisher<Result> &, INode &, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorParametersWithoutRunControl",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(IPublisher<Result> &, INode &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorResultOnly",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(IPublisher<Result> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
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
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutPublishProgress",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(IPublisher<Result> &, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::VoidFutureNode<Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
};

struct VoidFutureCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	VoidFutureCallbacksTestInstance,
	VoidFutureCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(VoidFutureCallbacksTest, RunAndWaitResult)
{
	const auto highway = hi::make_self_shared<SerialHighWay<>>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const TestData & test_params = GetParam();
	const auto node = test_params.creator_(promise, highway);
	node->execute();
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
