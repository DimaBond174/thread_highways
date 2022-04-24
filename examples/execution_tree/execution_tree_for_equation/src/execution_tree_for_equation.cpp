#include <thread_highways/include_all.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

template <std::uint32_t operands_count_>
struct SumCalculator : public hi::INode
{
	SumCalculator(
		std::weak_ptr<SumCalculator> self_weak,
		hi::IHighWayMailBoxPtr high_way_mail_box,
		hi::IPublisherPtr<hi::CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
	{
		auto future_node_logic = hi::AggregatingFutureNodeLogic<double, AggregatingBundle, double>::create(
			[this](
				std::uint32_t operand_id,
				double operand_value,
				AggregatingBundle & aggregating_bundle,
				std::uint32_t operands_count,
				hi::IPublisher<double> & result_publisher)
			{
				publish_progress_state(
					true,
					static_cast<std::uint32_t>(aggregating_bundle.operands.size()) * 100 / operands_count);
				aggregating_bundle.operands.emplace(operand_id, operand_value);
				if (aggregating_bundle.operands.size() < operands_count)
				{
					return;
				}

				double sum{0};
				for (auto && it : aggregating_bundle.operands)
				{
					sum += it.second;
				}

				result_publisher.publish(sum);

				publish_progress_state(false, 100);
				// готовимся считать новую сумму
				aggregating_bundle.operands.clear();
			},
			self_weak,
			__FILE__,
			__LINE__);

		calculator_ = hi::make_self_shared<hi::AggregatingFutureNode<double, AggregatingBundle, double>>(
			std::move(future_node_logic),
			high_way_mail_box);

		// подключаем каналы операндов
		for (std::uint32_t i = 0; i < operands_count_; ++i)
		{
			auto it = operands_.emplace_back(hi::make_self_shared<hi::PublishOneForMany<double>>());
			calculator_->add_operand_channel(*(it->subscribe_channel()));
		}
	}

	hi::ISubscribeHerePtr<double> result_channel()
	{
		return calculator_->result_channel();
	}

	struct AggregatingBundle
	{
		std::map<std::uint32_t, double> operands;
	};

	std::vector<std::shared_ptr<hi::PublishOneForMany<double>>> operands_;
	std::shared_ptr<hi::AggregatingFutureNode<double, AggregatingBundle, double>> calculator_;
};

/*
  result = (x1 + x2 + x3 ... + xn) / (y1 + y2 + y3 ... + ym) - (z1 + z2 + z3 ... + zk)
*/
void test_1()
{
	const auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	const auto highway_mailbox = highway->mailbox();

	hi::ExecutionTree execution_tree;

	auto sum_x_node =
		hi::make_self_shared<SumCalculator<300>>(highway_mailbox, execution_tree.current_executed_node_publisher(), 1);
	auto sum_y_node =
		hi::make_self_shared<SumCalculator<200>>(highway_mailbox, execution_tree.current_executed_node_publisher(), 2);
	auto sum_z_node =
		hi::make_self_shared<SumCalculator<100>>(highway_mailbox, execution_tree.current_executed_node_publisher(), 3);

	// операция деления суммы по X на сумму по Y
	auto division_logic_node = hi::make_self_shared<hi::OperationWithTwoOperandsFutureNode<double, double, double>>(
		hi::OperationWithTwoOperandsFutureNodeLogic<double, double, double>::create(
			[](double operand1, double operand2, hi::IPublisher<double> & result_publisher)
			{
				// todo сделать тест кейс где ловится деление на 0 в highway мониторе здоровья
				result_publisher.publish(operand1 / operand2);
			},
			highway->protector(),
			__FILE__,
			__LINE__),
		highway_mailbox,
		execution_tree.current_executed_node_publisher(),
		4);

	// операция вычитания из результата деления суммы по Z
	auto subtraction_logic_node = hi::make_self_shared<hi::OperationWithTwoOperandsFutureNode<double, double, double>>(
		hi::OperationWithTwoOperandsFutureNodeLogic<double, double, double>::create(
			[](double operand1, double operand2, hi::IPublisher<double> & result_publisher)
			{
				result_publisher.publish(operand1 - operand2);
			},
			highway->protector(),
			__FILE__,
			__LINE__),
		highway_mailbox,
		execution_tree.current_executed_node_publisher(),
		5);

	auto process_result_future_node = hi::make_self_shared<hi::FutureNodeWithProgressPublisher<double, double>>(
		hi::FutureNodeWithProgressPublisherLogic<double, double>::create(
			[](double result,
			   hi::IPublisher<double> & next,
			   const hi::PublishProgressStateCallback & publish_progress_state_callback,
			   const std::atomic<std::uint32_t> & global_run_id,
			   const std::uint32_t your_run_id)
			{
				for (uint16_t progress = 1;
					 progress < 100 && your_run_id == global_run_id.load(std::memory_order_acquire);
					 ++progress)
				{
					publish_progress_state_callback(true, progress);
				}
				publish_progress_state_callback(false, 100);
				next.publish(result);
			},
			highway->protector(),
			__FILE__,
			__LINE__),
		highway_mailbox,
		execution_tree.current_executed_node_publisher(),
		6);

	const auto result_future_node = hi::make_self_shared<hi::ResultWaitFutureNode<double>>(
		highway_mailbox,
		execution_tree.current_executed_node_publisher(),
		7);

	// соединяем части уравнения подписками
	// result = (x1 + x2 + x3 ... + xn) / (y1 + y2 + y3 ... + ym) - (z1 + z2 + z3 ... + zk)
	division_logic_node->add_operand1_channel(*sum_x_node->result_channel());
	division_logic_node->add_operand2_channel(*sum_y_node->result_channel());

	subtraction_logic_node->add_operand1_channel(*division_logic_node->result_channel());
	subtraction_logic_node->add_operand2_channel(*sum_z_node->result_channel());

	subtraction_logic_node->result_channel()->subscribe(process_result_future_node->subscription());
	process_result_future_node->result_channel()->subscribe(result_future_node->subscription());

	// сохраняем в дерево исполнения чтобы узлы не исчезли после выхода из скоупа
	execution_tree.add_node(sum_x_node);
	execution_tree.add_node(sum_y_node);
	execution_tree.add_node(sum_z_node);
	execution_tree.add_node(std::move(division_logic_node));
	execution_tree.add_node(std::move(subtraction_logic_node));
	execution_tree.add_node(std::move(process_result_future_node));
	execution_tree.add_node(result_future_node);
	execution_tree.current_executed_node_publisher()->subscribe(hi::Subscription<hi::CurrentExecutedNode>::create(
		hi::SubscriptionCallback<hi::CurrentExecutedNode>::create(
			[&](hi::CurrentExecutedNode publication, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				std::cout << "CurrentExecutedNode[" << publication.node_id_ << "] now executed("
						  << publication.in_progress_ << "), achieved_progress:" << publication.achieved_progress_
						  << std::endl;
			},
			highway->protector(),
			__FILE__,
			__LINE__),
		highway_mailbox));

	// теперь можно передавать только узел-результат:
	result_future_node->store_parent_execution_tree(std::move(execution_tree));

	// где-то когда-то запускаем уравнение на вычисление
	std::thread threadX(
		[&]()
		{
			double x = 1.0;
			for (auto && it : sum_x_node->operands_)
			{
				it->publish(++x);
			}
		});

	std::thread threadY(
		[&]()
		{
			double y = 1.0;
			for (auto && it : sum_y_node->operands_)
			{
				it->publish(++y);
			}
		});

	std::thread threadZ(
		[&]()
		{
			double z = 1.0;
			for (auto && it : sum_z_node->operands_)
			{
				it->publish(++z);
			}
		});

	threadX.join();
	threadY.join();
	threadZ.join();
	std::cout << "threadX, threadY, threadZ joined" << std::endl;
	const auto result = result_future_node->get_result();
	std::cout << "result = " << *result << std::endl;
} // test_1

int main(int /* argc */, char ** /* argv */)
{
	test_1();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
