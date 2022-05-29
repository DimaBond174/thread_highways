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

struct IfResult
{
	int if_result_;
};

struct ElseResult
{
	int else_result_;
};

struct TestData
{
	const std::string_view description_;
	std::function<std::shared_ptr<hi::IfElseFutureNode<Parameter, IfResult, ElseResult>>(
		std::reference_wrapper<std::promise<bool>>,
		IHighWayPtr)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"LaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](hi::IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>::LaunchParameters)
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
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](
					Parameter,
					IPublisher<IfResult> &,
					IPublisher<ElseResult> &,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
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
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &, INode &)
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
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](
					Parameter,
					IPublisher<IfResult> &,
					IPublisher<ElseResult> &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"OnlyIncomingAndOutcoming",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"OnlyIncomingAndIfResult",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				[promise](Parameter, IPublisher<IfResult> &)
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

				void operator()(hi::IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>::LaunchParameters)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
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

				void operator()(
					Parameter,
					IPublisher<IfResult> &,
					IPublisher<ElseResult> &,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
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

				void operator()(Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &, INode &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
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

				void operator()(
					Parameter,
					IPublisher<IfResult> &,
					IPublisher<ElseResult> &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorOnlyIncomingAndOutcoming",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorOnlyIncomingAndIfResult",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(Parameter, IPublisher<IfResult> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::IfElseFutureNode<Parameter, IfResult, ElseResult>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
};

struct IfElseFutureCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	IfElseFutureCallbacksTestInstance,
	IfElseFutureCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(IfElseFutureCallbacksTest, RunAndWaitResult)
{
	const auto highway = hi::make_self_shared<SerialHighWay<>>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const TestData & test_params = GetParam();
	const auto node = test_params.creator_(promise, highway);
	node->subscription().send(Parameter{1});
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
