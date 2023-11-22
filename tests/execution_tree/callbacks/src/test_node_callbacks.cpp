/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <future>
#include <mutex>
#include <string>

namespace hi
{
namespace
{

struct Operand
{
	int operand_;
};

struct Result
{
	int result_;
};

struct TestData
{
	const std::string_view description_;
	std::function<std::shared_ptr<InParamChannel<Operand>>(std::reference_wrapper<std::promise<bool>>, HighWayProxyPtr)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"DefaultNodeLambdaAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(
				[promise](
					Operand /*param*/,
					const std::int32_t /*label*/,
					hi::Publisher<Result> & /*publisher*/,
					const std::atomic<bool> & /*keep_execution*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"DefaultNodeLambdaWithout_keep_execution",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(
				[promise](Operand /*param*/, const std::int32_t /*label*/, hi::Publisher<Result> & /*publisher*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"DefaultNodeLambdaWithout_publisher",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(
				[promise](Operand /*param*/, const std::int32_t /*label*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"DefaultNodeLambdaWithout_label",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(
				[promise](Operand /*param*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"DefaultNodeSharedFunctorAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(
					Operand /*param*/,
					const std::int32_t /*label*/,
					hi::Publisher<Result> & /*publisher*/,
					const std::atomic<bool> & /*keep_execution*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"DefaultNodeSharedFunctorWithout_keep_execution",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(Operand /*param*/, const std::int32_t /*label*/, hi::Publisher<Result> & /*publisher*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"DefaultNodeSharedFunctorWithout_publisher",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(Operand /*param*/, const std::int32_t /*label*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"DefaultNodeSharedFunctorWithout_label",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(Operand /*param*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeDefaultNodeFabric<Operand, Result>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"ResultNodeLambdaAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(
				[promise](Operand /*param*/, const std::int32_t /*label*/, const std::atomic<bool> & /*keep_execution*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"ResultNodeLambdaWithout_keep_execution",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(
				[promise](Operand /*param*/, const std::int32_t /*label*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"ResultNodeLambdaWithout_label",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(
				[promise](Operand /*param*/)
				{
					promise.get().set_value(true);
				},
				highway);
		}},
	TestData{
		"ResultNodeFunctorAllParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(
					Operand /*param*/,
					const std::int32_t /*label*/,
					const std::atomic<bool> & /*keep_execution*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"ResultNodeFunctorWithout_keep_execution",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(Operand /*param*/, const std::int32_t /*label*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(std::move(logic), std::move(highway));
		}},
	TestData{
		"ResultNodeFunctorWithout_label",
		[](std::reference_wrapper<std::promise<bool>> promise, HighWayProxyPtr highway)
		{
			struct NodeLogic
			{
				NodeLogic(std::reference_wrapper<std::promise<bool>> promise)
					: promise_{promise}
				{
				}
				void operator()(Operand /*param*/)
				{
					promise_.get().set_value(true);
				}
				std::reference_wrapper<std::promise<bool>> promise_;
				std::mutex mutex_;
			};
			auto logic = std::make_shared<NodeLogic>(promise);
			return hi::ExecutionTreeResultNodeFabric<Operand>::create(std::move(logic), std::move(highway));
		}},
};

struct DefaultNodeCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	DefaultNodeCallbacksTestInstance,
	DefaultNodeCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(DefaultNodeCallbacksTest, RunAndWaitResultDirect)
{
	const TestData & test_params = GetParam();
	if (test_params.description_ == "DefaultNodeLambdaAllParameters"
		|| test_params.description_ == "DefaultNodeSharedFunctorAllParameters"
		|| test_params.description_ == "ResultNodeFunctorAllParameters"
		|| test_params.description_ == "ResultNodeLambdaAllParameters")
	{
		return;
	}
	const auto highway = hi::make_self_shared<HighWay>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const auto node = test_params.creator_(promise, hi::make_proxy(highway));
	const auto in_channel = node->in_param_direct_channel().lock();
	in_channel->send({12345});
	EXPECT_TRUE(future.get());

	highway->destroy();
}

TEST_P(DefaultNodeCallbacksTest, RunAndWaitResultOnHighway)
{
	const auto highway = hi::make_self_shared<HighWay>();

	std::promise<bool> promise;
	auto future = promise.get_future();

	const TestData & test_params = GetParam();
	const auto node = test_params.creator_(promise, hi::make_proxy(highway));
	const auto in_channel = node->in_param_highway_channel().lock();
	in_channel->send(Operand{12345});
	EXPECT_TRUE(future.get());

	highway->destroy();
}

} // namespace
} // namespace hi
