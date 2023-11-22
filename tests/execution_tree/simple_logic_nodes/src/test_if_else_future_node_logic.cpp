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
#include <string>

namespace hi
{

TEST(TestIfElseFutureNodeLogic, CheckNumberIsOddOrEvenDirectSend)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto weak_highway = make_proxy(*highway);

	const std::int32_t IfLogicBranch{111};
	const std::int32_t ElseLogicBranch{222};

	const auto if_then_node = hi::ExecutionTreeDefaultNodeFabric<std::int32_t, bool>::create(
		[&](std::uint32_t param, const std::int32_t /*label*/, hi::Publisher<bool> & publisher)
		{
			publisher.publish_direct(
				hi::LabeledPublication<bool>{true, ((param % 2) ? IfLogicBranch : ElseLogicBranch)});
		},
		weak_highway);

	const auto odd_result = hi::ExecutionTreeResultNodeFabric<bool>::create(weak_highway);
	const auto even_result = hi::ExecutionTreeResultNodeFabric<bool>::create(weak_highway);

	EXPECT_TRUE(if_then_node->connect_to_direct_channel<bool>(odd_result.get(), IfLogicBranch, IfLogicBranch));
	EXPECT_TRUE(if_then_node->connect_to_direct_channel<bool>(even_result.get(), ElseLogicBranch, ElseLogicBranch));

	for (std::uint32_t i = 0; i < 100; ++i)
	{
		odd_result->reset_result();
		even_result->reset_result();

		if_then_node->send_param_in_direct_channel(i);
		if (i % 2)
		{
			EXPECT_TRUE(odd_result->result_ready());
			EXPECT_FALSE(even_result->result_ready());

			EXPECT_TRUE(odd_result->get_result()->publication_);
		}
		else
		{
			EXPECT_FALSE(odd_result->result_ready());
			EXPECT_TRUE(even_result->result_ready());

			EXPECT_TRUE(even_result->get_result()->publication_);
		}
	}
}

TEST(TestIfElseFutureNodeLogic, CheckNumberIsOddOrEvenSendOnHighway)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto weak_highway = make_proxy(*highway);

	const std::int32_t IfLogicBranch{111};
	const std::int32_t ElseLogicBranch{222};

	const auto if_then_node = hi::ExecutionTreeDefaultNodeFabric<std::int32_t, bool>::create(
		[&](std::uint32_t param, const std::int32_t /*label*/, hi::Publisher<bool> & publisher)
		{
			publisher.publish_on_highway(
				hi::LabeledPublication<bool>{true, ((param % 2) ? IfLogicBranch : ElseLogicBranch)});
		},
		weak_highway);

	const auto odd_result = hi::ExecutionTreeResultNodeFabric<bool>::create(weak_highway);
	const auto even_result = hi::ExecutionTreeResultNodeFabric<bool>::create(weak_highway);

	if_then_node->connect_to_highway_channel<bool>(odd_result.get(), IfLogicBranch, IfLogicBranch);
	if_then_node->connect_to_highway_channel<bool>(even_result.get(), ElseLogicBranch, ElseLogicBranch);

	for (std::uint32_t i = 0; i < 100; ++i)
	{
		odd_result->reset_result();
		even_result->reset_result();

		if_then_node->send_param_in_highway_channel(i);
		if (i % 2)
		{
			EXPECT_TRUE(odd_result->get_result()->publication_);
			EXPECT_FALSE(even_result->result_ready());
		}
		else
		{
			EXPECT_TRUE(even_result->get_result()->publication_);
			EXPECT_FALSE(odd_result->result_ready());
		}
	}
}

} // namespace hi
