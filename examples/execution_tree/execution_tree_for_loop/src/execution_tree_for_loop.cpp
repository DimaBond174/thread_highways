#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("execution_tree_for_loop()");
	const auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	const auto highway_mailbox = highway->mailbox();

	const auto ifelse_logic_node = hi::IfElseFutureNode<double, double, std::string>::create(
		[&](double operand,
			hi::IPublisher<double> & if_result_publisher,
			hi::IPublisher<std::string> & else_result_publisher)
		{
			scope.print(std::string{"ifelse_logic_node received: "}.append(std::to_string(operand)));
			if (operand < 100.0)
			{
				if_result_publisher.publish(operand * operand);
			}
			else
			{
				else_result_publisher.publish(std::string("loop result = ").append(std::to_string(operand)));
			}
		},
		highway->protector_for_tests_only(),
		highway);

	// loop
	ifelse_logic_node->if_result_channel()->subscribe(ifelse_logic_node->subscription(false));

	// result waiter
	const auto result_future_node = hi::make_self_shared<hi::ResultWaitFutureNode<std::string>>(highway_mailbox);
	ifelse_logic_node->else_result_channel()->subscribe(result_future_node->subscription(true));

	// execute
	ifelse_logic_node->subscription().send(2.1);

	const auto result = result_future_node->get_result();
	scope.print(std::string{"result_future_node: "}.append(*result));

	std::cout << "Test finished" << std::endl;
	return 0;
}
