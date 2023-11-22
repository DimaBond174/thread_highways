/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <map>
#include <numeric>

namespace hi
{

TEST(TestAggregatingFutureNode, AggregateSumDirect)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	const std::int32_t expected_operands_count{10};

	class AggregatingBundle
	{
	public:
		void operator()(double publication, const std::int32_t label, hi::Publisher<double> & publisher)
		{
			operands_map_.try_emplace(label, publication);
			if (static_cast<std::int32_t>(operands_map_.size()) == expected_operands_count)
			{
				auto summ = std::accumulate(
					operands_map_.begin(),
					operands_map_.end(),
					0.0,
					[](const double previous, const std::pair<const std::int32_t, double> & p)
					{
						return previous + p.second;
					});
				publisher.publish_direct(summ);
			}
		}

	private:
		std::map<std::int32_t, double> operands_map_;
	};

	const auto proxy = hi::make_proxy(*highway);
	const auto aggregating_future = hi::make_self_shared<hi::DefaultNode<double, double, AggregatingBundle>>(proxy, 0);

	int calls{0};

	const auto result = hi::ExecutionTreeResultNodeFabric<double>::create(
		[&](double /*result*/)
		{
			++calls;
			EXPECT_EQ(calls, 1);
		},
		proxy,
		0);

	// подключем узел результат к узлу агрегации
	aggregating_future->connect_to_direct_channel<double>(result.get(), 0, 0);

	// подключаем каналы операндов
	std::vector<std::weak_ptr<ISubscription<double>>> operands;
	for (std::int32_t i = 0; i < expected_operands_count; ++i)
	{
		operands.emplace_back(aggregating_future->in_param_direct_channel(i));
	}

	// шлём операнды
	double value{1.1};
	double expected{0};
	for (auto && it : operands)
	{
		if (auto channel = it.lock())
		{
			channel->send(value);
		}
		expected += value;
		value *= 2.1;
	}

	EXPECT_EQ(expected, result->get_result()->publication_);
}

TEST(TestAggregatingFutureNode, AggregateSumOnHighway)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	const std::int32_t expected_operands_count{10};

	class AggregatingBundle
	{
	public:
		void operator()(double publication, const std::int32_t label, hi::Publisher<double> & publisher)
		{
			operands_map_.try_emplace(label, publication);
			if (static_cast<std::int32_t>(operands_map_.size()) == expected_operands_count)
			{
				auto summ = std::accumulate(
					operands_map_.begin(),
					operands_map_.end(),
					0.0,
					[](const double previous, const std::pair<const std::int32_t, double> & p)
					{
						return previous + p.second;
					});
				publisher.publish_on_highway(summ);
			}
		}

	private:
		std::map<std::int32_t, double> operands_map_;
	};

	const auto proxy = hi::make_proxy(*highway);
	const auto aggregating_future = hi::make_self_shared<hi::DefaultNode<double, double, AggregatingBundle>>(proxy, 0);

	std::atomic<int> calls{0};

	const auto result = hi::ExecutionTreeResultNodeFabric<double>::create(
		[&](double /*result*/)
		{
			++calls;
		},
		proxy,
		0);

	// подключем узел результат к узлу агрегации
	aggregating_future->connect_to_highway_channel<double>(result.get(), 0, 0);

	// подключаем каналы операндов
	std::vector<std::weak_ptr<ISubscription<double>>> operands;
	for (std::int32_t i = 0; i < expected_operands_count; ++i)
	{
		operands.emplace_back(aggregating_future->in_param_highway_channel(i));
	}

	// шлём операнды
	double value{1.1};
	double expected{0};
	for (auto && it : operands)
	{
		if (auto channel = it.lock())
		{
			channel->send(value);
		}
		expected += value;
		value *= 2.1;
	}

	EXPECT_EQ(expected, result->get_result()->publication_);
	EXPECT_EQ(calls, 1);
}

TEST(TestAggregatingFutureNode, AggregateInPreConfiguredObject)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	const auto proxy = hi::make_proxy(*highway);
	const std::int32_t expected_operands_count{10};

	class AggregatingBundle
	{
	public:
		AggregatingBundle(std::promise<double> result_promise)
			: result_promise_{std::move(result_promise)}
		{
		}

		void operator()(double publication, const std::int32_t label)
		{
			operands_map_.try_emplace(label, publication);
			if (static_cast<std::int32_t>(operands_map_.size()) == expected_operands_count)
			{
				auto summ = std::accumulate(
					operands_map_.begin(),
					operands_map_.end(),
					0.0,
					[](const double previous, const std::pair<const std::int32_t, double> & p)
					{
						return previous + p.second;
					});
				result_promise_.set_value(summ);
			}
		}

	private:
		std::map<std::int32_t, double> operands_map_;
		std::promise<double> result_promise_;
	};

	std::promise<double> result_promise;
	auto result_future = result_promise.get_future();
	AggregatingBundle pre_configured_object{std::move(result_promise)};
	const auto aggregating_future = hi::make_self_shared<hi::DefaultNode<double, double, AggregatingBundle>>(
		std::move(pre_configured_object),
		proxy,
		0);

	// подключаем каналы операндов
	std::vector<std::weak_ptr<ISubscription<double>>> operands;
	for (std::int32_t i = 0; i < expected_operands_count; ++i)
	{
		operands.emplace_back(aggregating_future->in_param_highway_channel(i));
	}

	// шлём операнды
	double value{1.1};
	double expected{0};
	for (auto && it : operands)
	{
		if (auto channel = it.lock())
		{
			channel->send(value);
		}
		expected += value;
		value *= 2.1;
	}

	EXPECT_EQ(expected, result_future.get());
}

} // namespace hi
