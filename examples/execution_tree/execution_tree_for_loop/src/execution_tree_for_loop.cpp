#include <thread_highways/include_all.h>

#include <iostream>

void test_1()
{
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	auto highway_mailbox = highway->mailbox();

	auto ifelse_logic_node = hi::make_self_shared<hi::IfElseFutureNode<double, double, std::string>>(
		hi::IfElseFutureNodeLogic<double, double, std::string>::create(
			[](double operand,
			   hi::IPublisher<double> & if_result_publisher,
			   hi::IPublisher<std::string> & else_result_publisher)
			{
				if (operand < 1000.0)
				{
					if_result_publisher.publish(operand * operand);
				}
				else
				{
					else_result_publisher.publish(std::string("loop result = ").append(std::to_string(operand)));
				}
			},
			highway->protector(),
			__FILE__,
			__LINE__),
		highway_mailbox);

	// loop
	ifelse_logic_node->if_result_channel()->subscribe(ifelse_logic_node->subscription(false));

	// result waiter
	auto result_future_node = hi::make_self_shared<hi::ResultWaitFutureNode<std::string>>(highway_mailbox);
	ifelse_logic_node->else_result_channel()->subscribe(result_future_node->subscription(true));

	// it is not necessary to save the block diagram of the algorithm in the tree
	// hi::ExecutionTree execution_tree;

	// execute
	ifelse_logic_node->subscription().send(2.0);

	const auto result = result_future_node->get_result();
	std::cout << "result_future_node result: " << *result << std::endl;
} // test_1

int main(int /* argc */, char ** /* argv */)
{
	test_1();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
