/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>

namespace hi
{

TEST(TestFutureNode, PublishDirectWork)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto weak_highway = make_proxy(*highway);
	const std::int32_t expected{100500};
	const auto future_node = hi::ExecutionTreeDefaultNodeFabric<std::int32_t, std::int32_t>::create(
		[](std::int32_t operand, const std::int32_t /* label */, hi::Publisher<std::int32_t> & publisher)
		{
			publisher.publish_direct(operand);
		},
		weak_highway,
		0);

	const auto result = hi::ExecutionTreeResultNodeFabric<std::int32_t>::create(weak_highway);
	// подключем узел результат к узлу
	EXPECT_TRUE(future_node->connect_to_direct_channel<std::int32_t>(result.get(), 0, 0));

	future_node->publish_direct(expected);

	EXPECT_EQ(result->get_result()->publication_, expected);
}

TEST(TestFutureNode, PublishOnHighwayWork)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto weak_highway = make_proxy(*highway);
	const std::int32_t expected{100500};
	const auto future_node = hi::ExecutionTreeDefaultNodeFabric<std::int32_t, std::int32_t>::create(
		[](std::int32_t operand, const std::int32_t /* label */, hi::Publisher<std::int32_t> & publisher)
		{
			publisher.publish_on_highway(operand);
		},
		weak_highway,
		0);

	const auto result = hi::ExecutionTreeResultNodeFabric<std::int32_t>::create(weak_highway);
	// подключем узел результат к узлу
	EXPECT_TRUE(future_node->connect_to_highway_channel<std::int32_t>(result.get(), 0, 0));

	future_node->publish_on_highway(expected);

	EXPECT_EQ(result->get_result()->publication_, expected);
}

TEST(TestFutureNode, PublishWorksAfterRemoveNode)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto weak_highway = make_proxy(*highway);
	const std::int32_t expected{100500};

	class NodeLogic
	{
	public:
		void operator()(double publication, const std::int32_t /* label */, hi::Publisher<double> & publisher)
		{
			publisher.publish_direct(publication);
		}
	};

	// Делаем из узлов блок схему в виде ромба
	const auto node0 = hi::make_self_shared<hi::DefaultNode<double, double, NodeLogic>>(weak_highway, 0);
	auto node1 = hi::make_self_shared<hi::DefaultNode<double, double, NodeLogic>>(weak_highway, 0);
	const auto node2 = hi::make_self_shared<hi::DefaultNode<double, double, NodeLogic>>(weak_highway, 0);

	const auto result = hi::ExecutionTreeResultNodeFabric<double>::create(weak_highway);

	node0->connect_to_direct_channel<double>(node1.get(), 0, 0);
	node0->connect_to_direct_channel<double>(node2.get(), 0, 0);

	node1->connect_to_direct_channel<double>(result.get(), 0, 0);
	node2->connect_to_direct_channel<double>(result.get(), 0, 0);

	// Сигнал должен пройти по одной из граней
	node0->publish_direct(expected);
	EXPECT_EQ(result->get_result()->publication_, expected);

	result->reset_result();
	node1.reset();

	// Сигнал должен пройти по оставшейся грани
	node0->publish_direct(expected);
	EXPECT_EQ(result->get_result()->publication_, expected);
}

} // namespace hi
