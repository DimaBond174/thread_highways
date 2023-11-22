#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("execution_tree_for_loop()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	hi::DefaultExecutionTree execution_tree{highway};

	const std::int32_t IfLogicBranch{111};
	const std::int32_t ElseLogicBranch{222};
	const std::int32_t LoopBranch{333};
	const std::int32_t AlterLabel{444};

	const auto ifelse_logic_node = hi::ExecutionTreeDefaultNodeFabric<double, std::string>::create(
		[&](double operand, const std::int32_t label, hi::Publisher<std::string> & publisher)
		{
			scope.print(std::string{"ifelse_logic_node received: "}
							.append(std::to_string(operand))
							.append(" with label: ")
							.append(std::to_string(label)));
			if (operand < 100.0)
			{
				publisher.publish_on_highway(
					hi::LabeledPublication<std::string>{"signal to if logic branch", IfLogicBranch});
			}
			else
			{
				publisher.publish_on_highway(
					hi::LabeledPublication<std::string>{"signal to else logic branch", ElseLogicBranch});
			}
		},
		execution_tree);

	// loop on "if" logic branch
	const auto loop_node = hi::ExecutionTreeDefaultNodeFabric<std::string, double>::create(
		[&, i = double{0.0}](std::string operand, const std::int32_t label, hi::Publisher<double> & publisher) mutable
		{
			scope.print(std::string{"loop_node received: \""}
							.append(operand)
							.append("\" with label: ")
							.append(std::to_string(label)));

			publisher.publish_on_highway(hi::LabeledPublication{++i, LoopBranch});
		},
		execution_tree);

	// loop exit on "else" logic branch
	const auto exit_node = hi::ExecutionTreeResultNodeFabric<std::string>::create(
		[&](const std::string & operand, const std::int32_t label) mutable
		{
			scope.print(std::string{"exit_node received: \""}
							.append(operand)
							.append("\" with label: ")
							.append(std::to_string(label)));
		},
		execution_tree);

	// connect nodes
	loop_node->subscribe_on_highway(ifelse_logic_node->in_param_highway_channel(LoopBranch, false), LoopBranch);

	ifelse_logic_node->subscribe_on_highway(loop_node->in_param_highway_channel(IfLogicBranch, false), IfLogicBranch);

	// Демонстрация того что можно получать сигналы на ветке IfLogicBranch, но промечать их как AlterLabel
	// в данном примере это уже вторая подписка, поэтому loop_node будет слать в 2 раза больше чем генерирует
	// ifelse_logic_node
	ifelse_logic_node->subscribe_on_highway(loop_node->in_param_highway_channel(AlterLabel, false), IfLogicBranch);
	ifelse_logic_node->subscribe_on_highway(
		exit_node->in_param_highway_channel(ElseLogicBranch, false),
		ElseLogicBranch);

	// execute
	ifelse_logic_node->send_param_in_highway_channel(0.1);

	const auto result = exit_node->get_result();
	scope.print(std::string{"exit_node->get_result(): "}.append(result->publication_));

	std::cout << "Test finished" << std::endl;
	return 0;
}
