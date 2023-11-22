/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <vector>

namespace hi
{

TEST(TestSelfDevelopingExecutionTree, SelfInsertDeleteNodesIntoExecutionTree)
{
	using namespace hi;
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

		struct AnyTypeData
		{
		};
		AnyTypeData data;
	};

	const auto create_neuron = [&](const std::int32_t node_id)
	{
		return hi::ExecutionTreeDefaultNodeFabric<Signal, Signal>::create(
			[&, node_id](Signal operand, const std::int32_t /* label */, hi::Publisher<Signal> & publisher) mutable
			{
				// label тут не используется, но он нужен для построения сложной if-else логики
				// он позволяет определить с какого узла пришли данные
				operand.trace.push_back(node_id);
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

	std::atomic<std::size_t> trace_length{0};
	const auto result_neuron = [&](const std::int32_t node_id)
	{
		return hi::ExecutionTreeResultNodeFabric<Signal>::create(
			[&, prev_id = root->node_id()](Signal operand) mutable
			{
				switch (operand.cmd)
				{
				case Signal::Cmd::Add:
					{
						if (operand.trace.size() > trace_length)
						{
							trace_length = operand.trace.size();
						}
						prev_id = add_neuron(prev_id);
						break;
					}
				case Signal::Cmd::Remove:
					{
						if (operand.trace.size() < trace_length)
						{
							trace_length = operand.trace.size();
						}
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
		return next_id;
	};

	remove_neuron = [&](const std::int32_t node_id) -> std::int32_t
	{
		result_neuron->delete_all_in_channels_direct();
		execution_tree.remove_node(node_id);
		const auto prev_id = node_id - 1;
		auto neuron = execution_tree.get_node(prev_id);
		neuron->connect_to_highway_channel<Signal>(result_neuron.get(), 0, 0);
		return prev_id;
	};

	root->connect_to_direct_channel<Signal>(result_neuron.get(), 0, 0);

	for (int i = 0; i < 5; ++i)
	{
		highway->flush_tasks();
		result_neuron->reset_result();
		root->send_param_in_highway_channel(Signal{Signal::Cmd::Add, {}, {}});
		result_neuron->get_result();
	}

	const std::size_t achived_length = trace_length;
	EXPECT_GT(achived_length, 0);

	for (int i = 0; i < 10; ++i)
	{
		highway->flush_tasks();
		result_neuron->reset_result();
		root->send_param_in_highway_channel(Signal{Signal::Cmd::Remove, {}, {}});
		result_neuron->get_result();
	}
	EXPECT_GT(achived_length, trace_length);

	highway->destroy();
}

} // namespace hi
