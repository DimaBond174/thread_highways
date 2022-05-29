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

struct Operand1
{
	int operand1_;
};

struct Operand2
{
	int operand2_;
};

struct Result
{
	int result_publisher_;
};

struct TestData
{
	const std::string_view description_;
	std::function<INodePtr(
		std::reference_wrapper<std::promise<bool>>,
		IHighWayPtr,
		ISubscribeHere<Operand1> &,
		ISubscribeHere<Operand2> &)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"LaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](hi::OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>::LaunchParameters)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"AllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](
					Operand1,
					Operand2,
					IPublisher<Result> &,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"WithoutRunControl",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](Operand1, Operand2, IPublisher<Result> &, INode &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"WithoutProgressPublisher",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](
					Operand1,
					Operand2,
					IPublisher<Result> &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"OperandsAndResult",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](Operand1, Operand2, IPublisher<Result> &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"OperandsOnly",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				[promise](Operand1, Operand2)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorLaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(
					hi::OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>::LaunchParameters)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(
					Operand1,
					Operand2,
					IPublisher<Result> &,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorWithoutRunControl",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(Operand1, Operand2, IPublisher<Result> &, INode &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorWithoutProgressPublisher",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(
					Operand1,
					Operand2,
					IPublisher<Result> &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorOperandsAndResult",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(Operand1, Operand2, IPublisher<Result> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
	TestData{
		"FunctorOperandsOnly",
		[](std::reference_wrapper<std::promise<bool>> promise,
		   IHighWayPtr highway,
		   ISubscribeHere<Operand1> & operand1_channel,
		   ISubscribeHere<Operand2> & operand2_channel) -> INodePtr
		{
			struct Fun
			{
				Fun(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{std::move(promise)}
				{
				}

				void operator()(Operand1, Operand2)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway,
				operand1_channel,
				operand2_channel);
		}},
};

struct OperationWithTwoOperandsFutureCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	OperationWithTwoOperandsFutureCallbacksTestInstance,
	OperationWithTwoOperandsFutureCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(OperationWithTwoOperandsFutureCallbacksTest, RunAndWaitResult)
{
	const auto highway = hi::make_self_shared<SerialHighWay<>>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	auto operand1_publisher = hi::make_self_shared<PublishOneForMany<Operand1>>();
	auto operand2_publisher = hi::make_self_shared<PublishOneForMany<Operand2>>();

	const TestData & test_params = GetParam();
	const auto node = test_params.creator_(
		promise,
		highway,
		*operand1_publisher->subscribe_channel(),
		*operand2_publisher->subscribe_channel());
	operand1_publisher->publish(Operand1{1});
	operand2_publisher->publish(Operand2{2});
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
