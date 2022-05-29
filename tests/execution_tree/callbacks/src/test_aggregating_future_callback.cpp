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

struct Operand
{
	int operand_;
};

struct AggregatingBundle
{
	int aggregating_bundle_;
};

struct Result
{
	int result_;
};

struct TestData
{
	const std::string_view description_;
	std::function<std::shared_ptr<hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>>(
		std::reference_wrapper<std::promise<bool>>,
		IHighWayPtr)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"LaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](hi::AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>::LaunchParameters)
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
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](
					std::uint32_t,
					Operand,
					AggregatingBundle &,
					IPublisher<Result> &,
					std::uint32_t,
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
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &, std::uint32_t, INode &)
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
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &, std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutOperandsCount",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutResultPublisher",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				[promise](std::uint32_t, Operand, AggregatingBundle &)
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

				void operator()(hi::AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>::LaunchParameters)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
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
					std::uint32_t,
					Operand,
					AggregatingBundle &,
					IPublisher<Result> &,
					std::uint32_t,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
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

				void
				operator()(std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &, std::uint32_t, INode &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
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

				void operator()(std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &, std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutOperandsCount",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutResultPublisher",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(std::uint32_t, Operand, AggregatingBundle &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::AggregatingFutureNode<Operand, AggregatingBundle, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
};

struct AggregatingFutureCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	AggregatingFutureCallbacksTestInstance,
	AggregatingFutureCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(AggregatingFutureCallbacksTest, RunAndWaitResult)
{
	const auto highway = hi::make_self_shared<SerialHighWay<>>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const TestData & test_params = GetParam();
	const auto aggregating_future = test_params.creator_(promise, highway);
	const auto operand_channel = hi::make_self_shared<hi::PublishOneForMany<Operand>>();
	aggregating_future->add_operand_channel(operand_channel->subscribe_channel());
	operand_channel->publish(Operand{1});
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
