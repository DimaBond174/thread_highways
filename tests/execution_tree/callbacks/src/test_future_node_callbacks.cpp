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
	std::function<
		std::shared_ptr<hi::FutureNode<Parameter, Result>>(std::reference_wrapper<std::promise<bool>>, IHighWayPtr)>
		creator_;
};

TestData test_data[] = {
	TestData{
		"LaunchParameters",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::FutureNode<Parameter, Result>::create(
				[promise](hi::FutureNodeLogic<Parameter, Result>::LaunchParameters)
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
			return hi::FutureNode<Parameter, Result>::create(
				[promise](
					Parameter,
					IPublisher<Result> &,
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
			return hi::FutureNode<Parameter, Result>::create(
				[promise](Parameter, IPublisher<Result> &, INode &)
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
			return hi::FutureNode<Parameter, Result>::create(
				[promise](Parameter, IPublisher<Result> &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutIncomingParameterWithRunControl",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::FutureNode<Parameter, Result>::create(
				[promise](IPublisher<Result> &, INode &, const std::atomic<std::uint32_t> &, const std::uint32_t)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"WithoutIncomingParameter",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::FutureNode<Parameter, Result>::create(
				[promise](IPublisher<Result> &, INode &)
				{
					promise.get().set_value(true);
				},
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"OnlyResultPublisher",
		[](std::reference_wrapper<std::promise<bool>> promise, IHighWayPtr highway)
		{
			return hi::FutureNode<Parameter, Result>::create(
				[promise](IPublisher<Result> &)
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

				void operator()(hi::FutureNodeLogic<Parameter, Result>::LaunchParameters)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::FutureNode<Parameter, Result>::create(
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
					IPublisher<Result> &,
					INode &,
					const std::atomic<std::uint32_t> &,
					const std::uint32_t)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::FutureNode<Parameter, Result>::create(
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

				void operator()(Parameter, IPublisher<Result> &, INode &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::FutureNode<Parameter, Result>::create(
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

				void operator()(Parameter, IPublisher<Result> &)
				{
					promise_.get().set_value(true);
				}

				std::reference_wrapper<std::promise<bool>> promise_;
			};
			return hi::FutureNode<Parameter, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutIncomingParameterWithRunControl",
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
			return hi::FutureNode<Parameter, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorWithoutIncomingParameter",
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
			return hi::FutureNode<Parameter, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
	TestData{
		"FunctorOnlyResultPublisher",
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
			return hi::FutureNode<Parameter, Result>::create(
				std::make_unique<Fun>(promise),
				highway->protector_for_tests_only(),
				highway);
		}},
};

struct FutureCallbacksTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	FutureCallbacksTestInstance,
	FutureCallbacksTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(FutureCallbacksTest, RunAndWaitResult)
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
