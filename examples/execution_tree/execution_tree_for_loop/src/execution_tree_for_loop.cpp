#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("execution_tree_for_loop()");
    const auto highway = hi::make_self_shared<hi::HighWay>();
    hi::DefaultExecutionTree execution_tree{highway};

    const std::int32_t IfLogicBranch{111};
    const std::int32_t ElseLogicBranch{222};

    const auto ifelse_logic_node = hi::ExecutionTreeDefaultNodeFabric<double, std::string>::create(
        [&](hi::LabeledPublication<double> operand)
		{
            scope.print(std::string{"ifelse_logic_node received: "}.append(std::to_string(operand.publication_)));
            if (operand.publication_ < 100.0)
			{
                return hi::LabeledPublication<std::string>{"signal to if logic branch", IfLogicBranch};
			}
			else
			{
                return hi::LabeledPublication<std::string>{"signal to else logic branch", ElseLogicBranch};
			}
        }, execution_tree);

    // loop on "if" logic branch
    const auto loop_node = hi::ExecutionTreeDefaultNodeFabric<std::string, double>::create(
                [&, i = double{0.0}](hi::LabeledPublication<std::string> operand) mutable
        {
            scope.print(std::string{"loop_node received: "}.append(operand.publication_));
            return ++i;
        }, execution_tree);

    // loop exit on "else" logic branch
    const auto exit_node = hi::ExecutionTreeResultNodeFabric<std::string>::create(
                [&](hi::LabeledPublication<std::string> operand) mutable
        {
            scope.print(std::string{"exit_node received: "}.append(operand.publication_));
        }, execution_tree);

    // connect nodes
    loop_node->subscribe(ifelse_logic_node->in_param_channel());

    ifelse_logic_node->subscribe(loop_node->in_param_channel(), IfLogicBranch);
    ifelse_logic_node->subscribe(exit_node->in_param_channel(), ElseLogicBranch);

	// execute
    ifelse_logic_node->execute();

    const auto result = exit_node->get_result();
    scope.print(std::string{"exit_node->get_result(): "}.append(result->publication_));

	std::cout << "Test finished" << std::endl;
	return 0;
}
