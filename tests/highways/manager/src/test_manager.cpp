/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <functional>
#include <stack>
#include <unordered_map>

namespace hi
{

using namespace std::chrono_literals;

struct TestDataParallelCalculations
{
	std::string_view description_;
	std::int32_t threads_cnt_;
	std::shared_ptr<hi::HighWaysManager> highways_manager_;
};

TestDataParallelCalculations test_data[] = {
	TestDataParallelCalculations{"NoTimeControl", 64, hi::make_self_shared<hi::HighWaysManager>(1u, 2u)},
	TestDataParallelCalculations{
		"WithTimeControl",
		32,
		hi::make_self_shared<hi::HighWaysManager>(
			1u,
			1u,
			false,
			[](const hi::Exception & ex)
			{
				throw ex;
			},
			"HighWaysManager",
			65000u,
			hi::HighWaysManager::HighWaySettings{std::chrono::milliseconds{1000}, 65000u})},
};

struct ParallelCalculationsTest : public ::testing::TestWithParam<TestDataParallelCalculations>
{
};

INSTANTIATE_TEST_SUITE_P(
	ParallelCalculationsTestInstance,
	ParallelCalculationsTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestDataParallelCalculations> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(ParallelCalculationsTest, ExpectedResult)
{
	const TestDataParallelCalculations & test_params = GetParam();

	struct Data
	{
		Data()
			: result_future_{result_.get_future()} {};

		std::uint32_t data_{};
		std::promise<std::uint32_t> result_;
		std::future<std::uint32_t> result_future_;
	};
	using DataPtr = std::shared_ptr<Data>;

	struct CalculationsResult
	{
		DataPtr data_;
		std::uint32_t result_{};
	};

	class AggregatingBundle
	{
	public:
		AggregatingBundle(std::int32_t threads_cnt)
			: threads_cnt_{threads_cnt}
		{
		}

		void operator()(
			CalculationsResult сalculations_result,
			const std::int32_t label,
			hi::Publisher<CalculationsResult> & publisher)
		{
			if (0 == label)
			{ // это сигнал с main thread
				// Запускаю на многопоточку распределённые вычисления
				publisher.publish_on_highway(сalculations_result);
				return;
			}

			auto & cnt = operands_map_[сalculations_result.data_];
			cnt += 1;
			сalculations_result.data_->data_ += сalculations_result.result_;

			if (cnt == threads_cnt_)
			{
				сalculations_result.data_->result_.set_value(сalculations_result.data_->data_);
			}
		}

	private:
		const std::int32_t threads_cnt_;
		std::unordered_map<DataPtr, std::int32_t> operands_map_;
	};

	auto aggregating_node = hi::ExecutionTreeDefaultNodeFabric<CalculationsResult, CalculationsResult>::create(
		AggregatingBundle{test_params.threads_cnt_},
		test_params.highways_manager_->get_highway(100));

	std::vector<std::shared_ptr<hi::ISubscription<CalculationsResult>>> incoming_channels;
	const std::int32_t channels_cnt = test_params.threads_cnt_ + 1; // [0] for main thread
	// Создаю каналы через которые будут приходить данные.
	// Можно обойтись и одним каналом если нет необходимости знать идентификатор источника входящих данных.
	for (std::int32_t i = 0; i < channels_cnt; ++i)
	{
		incoming_channels.emplace_back(aggregating_node->in_param_highway_channel(i, false).lock());
	}

	auto execute_parallel = [&](CalculationsResult data)
	{
		for (std::int32_t i = 1; i < channels_cnt; ++i)
		{
			test_params.highways_manager_->execute(
				[&, base_data = data.result_, i, data]
				{
					std::this_thread::sleep_for(100ms); // сложные вычисления
					auto result = base_data + i;
					incoming_channels[i]->send(CalculationsResult{data.data_, result});
				});
		}
	};

	// Зацикливаю в круг: результат aggregating_node запустит новые параллельные вычисления
	auto subscription = hi::create_subscription<CalculationsResult>(
		[&](CalculationsResult result)
		{
			execute_parallel(std::move(result));
		});
	aggregating_node->subscribe_direct(subscription);

	// Вхожу в цикл с данными:
	std::vector<DataPtr> datas;
	for (std::int32_t j = 0; j < 3; ++j)
	{
		auto data = std::make_shared<Data>();
		datas.emplace_back(data);
		incoming_channels[0]->send(CalculationsResult{data, 0});
	}

	std::uint32_t expected{0};
	for (std::uint32_t i = 1; i <= static_cast<std::uint32_t>(test_params.threads_cnt_); ++i)
	{
		expected += i;
	}

	// Встаю на ожидание результатов:
	for (auto it : datas)
	{
		auto res = it->result_future_.get();
		EXPECT_EQ(res, expected);
	}
}

struct TestDataLoadBalancing
{
	std::string_view description_;
	std::int32_t cnt_;
	std::function<bool(std::size_t min_size, std::size_t max_size)> check;
	std::shared_ptr<hi::HighWaysManager> highways_manager_;
};

TestDataLoadBalancing test_data2[] = {
	TestDataLoadBalancing{
		"NoAutoRegulation",
		10,
		[](std::size_t min_size, std::size_t max_size) -> bool
		{
			return min_size == 2 && max_size == 2;
		},
		hi::make_self_shared<hi::HighWaysManager>(1u, 1u, false)},
	TestDataLoadBalancing{
		"WithAutoRegulation",
		10,
		[](std::size_t min_size, std::size_t max_size) -> bool
		{
			return min_size == 2 && max_size > 2;
		},
		hi::make_self_shared<hi::HighWaysManager>(1u, 1u, true)},
};

struct LoadBalancingTest : public ::testing::TestWithParam<TestDataLoadBalancing>
{
};

INSTANTIATE_TEST_SUITE_P(
	LoadBalancingTestInstance,
	LoadBalancingTest,
	::testing::ValuesIn(test_data2),
	[](const ::testing::TestParamInfo<TestDataLoadBalancing> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(LoadBalancingTest, ExpectedResult)
{
	const TestDataLoadBalancing & test_params = GetParam();

	std::atomic<std::int32_t> executed{test_params.cnt_};
	std::atomic<std::size_t> max_size{0};
	auto check_size = [&]
	{
		auto size = test_params.highways_manager_->size();
		if (max_size < size)
			max_size = size;
	};

	std::stack<hi::HighWayProxyPtr> highways;
	for (std::int32_t i = 0; i < test_params.cnt_; ++i)
	{
		auto highway = test_params.highways_manager_->get_highway(45);
		highways.emplace(highway);
		check_size();

		highway->execute(
			[&]()
			{
				std::this_thread::sleep_for(100ms);
				--executed;
				check_size();
			});
	}

	while (executed > 0)
	{
		std::this_thread::sleep_for(100ms);
	}

	while (!highways.empty())
	{
		highways.pop();
		check_size();
	}

	EXPECT_TRUE(test_params.check(test_params.highways_manager_->size(), max_size));
}

} // namespace hi
