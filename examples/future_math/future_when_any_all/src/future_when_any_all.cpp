#include <thread_highways/include_all.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

void test_when_all()
{
	//	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	//	auto highway_mailbox = highway->mailbox();

	//	struct AggregatingBundle
	//	{
	//		std::map<std::uint32_t, std::int32_t> operands;
	//	};

	//	auto future_node_logic = hi::AggregatingFutureNodeLogic<std::int32_t, AggregatingBundle, std::string>::create(
	//		[&](std::uint32_t operand_id,
	//			std::int32_t operand_value,
	//			AggregatingBundle & aggregating_bundle,
	//			std::uint32_t operands_count,
	//			hi::IPublisher<std::string> & result_publisher)
	//		{
	//			assert(highway->current_execution_on_this_highway());
	//			assert(aggregating_bundle.operands.find(operand_id) == aggregating_bundle.operands.end());
	//			aggregating_bundle.operands.emplace(operand_id, operand_value);
	//			if (aggregating_bundle.operands.size() < operands_count)
	//			{
	//				// вообще, тут не обязательно ждать когда все операнды появятся - для некоторых задач например 50%
	//				// хватит
	//				return;
	//			}

	//			std::int32_t sum{0};
	//			for (auto && it : aggregating_bundle.operands)
	//			{
	//				sum += it.second;
	//			}

	//			// тип операндов и результата может не совпадать
	//			result_publisher.publish(std::string("result sum = ").append(std::to_string(sum)));

	//			// готовимся считать новую сумму
	//			aggregating_bundle.operands.clear();
	//		},
	//		highway->protector_for_tests_only(),
	//		__FILE__,
	//		__LINE__);

	//	auto future_node = hi::make_self_shared<hi::AggregatingFutureNode<std::int32_t, AggregatingBundle,
	//std::string>>( 		std::move(future_node_logic), 		highway_mailbox);

	//	// подписываемся на результат
	//	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
	//		[&](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
	//		{
	//			std::cout << "received message: " << publication << std::endl;
	//		},
	//		highway->protector_for_tests_only(),
	//		__FILE__,
	//		__LINE__);

	//	future_node->result_channel()->subscribe(
	//		hi::Subscription<std::string>::create(std::move(subscription_callback), highway_mailbox));

	//	// подключаем каналы операндов
	//	std::uint32_t operands_count{10};
	//	std::vector<std::shared_ptr<hi::PublishOneForMany<std::int32_t>>> operands;
	//	for (std::uint32_t i = 0; i < operands_count; ++i)
	//	{
	//		auto it = operands.emplace_back(hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>());
	//		future_node->add_operand_channel(*(it->subscribe_channel()));
	//	}

	//	// шлём операнды
	//	for (std::int32_t value = 1; value < 100; ++value)
	//	{
	//		for (auto && it : operands)
	//		{
	//			it->publish(value);
	//		}
	//	}
	//	highway->flush_tasks();
	//	highway->destroy();
} // test_when_all

void test_when_any()
{
	//	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	//	auto highway_mailbox = highway->mailbox();

	//	struct AggregatingBundle
	//	{
	//		bool was_sent_{false};
	//	};

	//	auto future_node_logic = hi::AggregatingFutureNodeLogic<std::int32_t, AggregatingBundle, std::string>::create(
	//		[&](std::uint32_t operand_id,
	//			std::int32_t operand_value,
	//			AggregatingBundle & aggregating_bundle,
	//			std::uint32_t operands_count,
	//			hi::IPublisher<std::string> & result_publisher)
	//		{
	//			if (aggregating_bundle.was_sent_)
	//			{
	//				return;
	//			}
	//			aggregating_bundle.was_sent_ = true;
	//			// тип операндов и результата может не совпадать
	//			result_publisher.publish(std::string("result = ")
	//										 .append(std::to_string(operand_value))
	//										 .append("\n was received from:")
	//										 .append(std::to_string(operand_id))
	//										 .append("\n all connections count:")
	//										 .append(std::to_string(operands_count)));
	//		},
	//		highway->protector_for_tests_only(),
	//		__FILE__,
	//		__LINE__);

	//	auto future_node = hi::make_self_shared<hi::AggregatingFutureNode<std::int32_t, AggregatingBundle,
	//std::string>>( 		std::move(future_node_logic), 		highway_mailbox);

	//	// подписываемся на результат
	//	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
	//		[&](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
	//		{
	//			std::cout << "received message: " << publication << std::endl;
	//		},
	//		highway->protector_for_tests_only(),
	//		__FILE__,
	//		__LINE__);

	//	future_node->result_channel()->subscribe(
	//		hi::Subscription<std::string>::create(std::move(subscription_callback), highway_mailbox));

	//	// подключаем каналы операндов
	//	std::uint32_t operands_count{100};
	//	std::vector<std::shared_ptr<hi::PublishOneForMany<std::int32_t>>> operands;
	//	for (std::uint32_t i = 0; i < operands_count; ++i)
	//	{
	//		auto it = operands.emplace_back(hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>());
	//		future_node->add_operand_channel(*(it->subscribe_channel()));
	//	}

	//	// шлём операнды
	//	for (auto && it : operands)
	//	{
	//		it->publish(100500);
	//	}
	//	highway->flush_tasks();
	//	highway->destroy();
} // test_when_any

int main(int /* argc */, char ** /* argv */)
{
	test_when_all();
	test_when_any();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
