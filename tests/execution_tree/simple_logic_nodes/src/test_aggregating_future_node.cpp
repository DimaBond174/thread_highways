#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <map>
#include <numeric>

namespace hi
{

TEST(TestAggregatingFutureNode, AggregateSum)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};

	struct AggregatingBundle
	{
		// no need in mutex because of hi::SerialHighWay
		std::map<std::uint32_t, double> operands_map_;
	};

	const std::uint32_t expected_operands_count{10};
	auto aggregating_future = AggregatingFutureNode<double, AggregatingBundle, double>::create(
		[expected_operands_count](
			const std::uint32_t operand_id,
			const double operand_value,
			AggregatingBundle & aggregating_bundle,
			hi::IPublisher<double> & result_publisher,
			const std::uint32_t operands_count)
		{
			aggregating_bundle.operands_map_.try_emplace(operand_id, operand_value);
			if (static_cast<std::uint32_t>(aggregating_bundle.operands_map_.size()) == operands_count)
			{
				EXPECT_EQ(expected_operands_count, operands_count);
				auto summ = std::accumulate(
					aggregating_bundle.operands_map_.begin(),
					aggregating_bundle.operands_map_.end(),
					0.0,
					[](const double previous, const std::pair<const std::uint32_t, double> & p)
					{
						return previous + p.second;
					});
				result_publisher.publish(summ);
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			std::shared_ptr<AggregatingFutureNode<double, AggregatingBundle, double>> node,
			IHighWayMailBoxPtr highway_mailbox)
			: future_{promise_.get_future()}
		{
			hi::subscribe(
				*node->result_channel(),
				[this](double result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					promise_.set_value(result);
				},
				self_weak,
				highway_mailbox);
		}

		std::promise<double> promise_;
		std::future<double> future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	auto mailbox = highway.object_->mailbox();
	auto checker = hi::make_self_shared<SelfProtectedChecker>(aggregating_future, mailbox);

	// подключаем каналы операндов
	std::vector<std::shared_ptr<hi::PublishOneForMany<double>>> operands;
	for (std::uint32_t i = 0; i < expected_operands_count; ++i)
	{
		auto it = operands.emplace_back(hi::make_self_shared<hi::PublishOneForMany<double>>());
		aggregating_future->add_operand_channel(*(it->subscribe_channel()));
	}

	// шлём операнды
	double expected{0};
	double value{1.1};
	for (auto && it : operands)
	{
		it->publish(value);
		expected += value;
		value *= 2.1;
	}

	EXPECT_EQ(expected, checker->future_.get());
	EXPECT_EQ(1, checker->exec_counter_);
}

} // namespace hi
