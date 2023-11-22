#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <functional>

/*
	Демострация динамического изменения блок-схемы алгоритма во время его исполнения.
	Как мозг может менять связи между нейронами, так и узлы ExecutionTree могут добавлять
	новые узлы, настраивая каналы связи между собой.

	На этом максимально простом примере цепочка узлов сама себя выращивает, а затем сокращает.
*/
int main(int /* argc */, char ** /* argv */)
{
	using namespace hi;
	CoutScope scope("neural_network()");
	RAIIdestroy highway{make_self_shared<HighWay>()};
	auto weak_highway = make_proxy(*highway);
	DefaultExecutionTree execution_tree{weak_highway};

	struct Signal
	{
		enum class Cmd
		{
			None,
			Add,
			Remove
		};
		Cmd cmd{Cmd::None};

		std::vector<std::int32_t> trace;

		/*
		 * Большие данные которые дорого копировать можно положить в shared_ptr
		 *  или сделать как в примере wifi_beacon_frames_parser.cpp передачу по ссылке без перепланирования обработки на
		 * потоках
		 */
		struct AnyTypeData
		{
		};
		AnyTypeData data;
	};

	const auto print_trace = [&](std::string node_id, const std::vector<std::int32_t> & trace)
	{
		std::string str{"Trace["};
		str.append(node_id).append("]:");
		for (auto it : trace)
		{
			str.append(std::to_string(it)).append(",");
		}
		scope.print(str);
	};

	const auto create_neuron = [&](const std::int32_t node_id)
	{
		return hi::ExecutionTreeDefaultNodeFabric<Signal, Signal>::create(
			[&, node_id](Signal operand, const std::int32_t /* label */, hi::Publisher<Signal> & publisher) mutable
			{
				// label тут не используется, но он нужен для построения сложной if-else логики
				// он позволяет определить с какого узла пришли данные
				operand.trace.push_back(node_id);
				print_trace(std::to_string(node_id), operand.trace);
				publisher.publish_on_highway(hi::LabeledPublication{std::move(operand)});
			},
			execution_tree,
			weak_highway,
			node_id);
	};

	const auto result_node_id = execution_tree.generate_node_id();
	const auto root_node_id = execution_tree.generate_node_id();
	const auto root = create_neuron(root_node_id);

	std::function<std::int32_t(std::int32_t)> add_neuron;
	std::function<std::int32_t(std::int32_t)> remove_neuron;

	const auto result_neuron = [&](const std::int32_t node_id)
	{
		return hi::ExecutionTreeResultNodeFabric<Signal>::create(
			[&, prev_id = root->node_id()](Signal operand) mutable
			{
				print_trace("result_neuron", operand.trace);
				switch (operand.cmd)
				{
				case Signal::Cmd::Add:
					{
						prev_id = add_neuron(prev_id);
						break;
					}
				case Signal::Cmd::Remove:
					{
						if (prev_id < 2)
						{
							break;
						}
						prev_id = remove_neuron(prev_id);
						break;
					}
				default:
					break;
				}
			},
			execution_tree,
			weak_highway,
			node_id);
	}(result_node_id);

	add_neuron = [&](const std::int32_t node_id) -> std::int32_t
	{
		result_neuron->delete_all_in_channels_direct();
		auto prev_neuron = execution_tree.get_node(node_id);
		const auto next_id = execution_tree.generate_node_id();
		auto new_neuron = create_neuron(next_id);
		prev_neuron->connect_to_highway_channel<Signal>(new_neuron.get(), 0, 0);
		new_neuron->connect_to_highway_channel<Signal>(result_neuron.get(), 0, 0);
		scope.print(std::string{"add_neuron{"}.append(std::to_string(next_id)).append("}"));
		return next_id;
	};

	remove_neuron = [&](const std::int32_t node_id) -> std::int32_t
	{
		result_neuron->delete_all_in_channels_direct();
		execution_tree.remove_node(node_id);
		const auto prev_id = node_id - 1;
		auto neuron = execution_tree.get_node(prev_id);
		neuron->connect_to_highway_channel<Signal>(result_neuron.get(), 0, 0);
		scope.print(std::string{"remove_neuron{"}.append(std::to_string(node_id)).append("}"));
		return prev_id;
	};

	root->connect_to_highway_channel<Signal>(result_neuron.get(), 0, 0);

	for (int i = 0; i < 5; ++i)
	{
		highway->flush_tasks();
		result_neuron->reset_result();
		root->send_param_in_highway_channel(Signal{Signal::Cmd::Add, {}, {}});
		result_neuron->get_result();
	}

	std::this_thread::sleep_for(std::chrono::seconds(1));

	for (int i = 0; i < 10; ++i)
	{
		highway->flush_tasks();
		result_neuron->reset_result();
		root->send_param_in_highway_channel(Signal{Signal::Cmd::Remove, {}, {}});
		result_neuron->get_result();
	}

	highway->destroy();
	return 0;
}
