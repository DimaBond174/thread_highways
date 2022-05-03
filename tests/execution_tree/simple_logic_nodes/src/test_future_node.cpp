#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>

namespace hi
{

TEST(TestFutureNode, Callback1)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};
	const std::int32_t expected{100500};
	auto future_node = hi::FutureNode<std::int32_t, std::int32_t>::create(
		[&](hi::IPublisher<std::int32_t> & result_publisher)
		{
			result_publisher.publish(expected);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	std::promise<std::int32_t> future_result;
	auto future_result_ready = future_result.get_future();

	hi::subscribe(
		*future_node->result_channel(),
		[&](std::int32_t publication)
		{
			future_result.set_value(publication);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	future_node->execute();
	EXPECT_EQ(future_result_ready.get(), expected);
}

TEST(TestFutureNode, Callback2)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};
	const std::int32_t expected{100500};
	auto future_node = hi::FutureNode<std::int32_t, std::int32_t>::create(
		[&](std::int32_t param, hi::IPublisher<std::int32_t> & result_publisher)
		{
			result_publisher.publish(param);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	std::promise<std::int32_t> future_result;
	auto future_result_ready = future_result.get_future();

	hi::subscribe(
		*future_node->result_channel(),
		[&](std::int32_t publication) mutable
		{
			future_result.set_value(publication);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	future_node->subscription().send(expected);
	EXPECT_EQ(future_result_ready.get(), expected);
}

TEST(TestFutureNode, Callback3)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};
	const std::int32_t expected{100500};
	auto future_node = hi::FutureNode<std::int32_t, std::int32_t>::create(
		[&](std::int32_t param,
			hi::IPublisher<std::int32_t> & result_publisher,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id)
		{
			EXPECT_EQ(global_run_id, your_run_id);
			result_publisher.publish(param);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	std::promise<std::int32_t> future_result;
	auto future_result_ready = future_result.get_future();

	hi::subscribe(
		*future_node->result_channel(),
		[&](std::int32_t publication) mutable
		{
			future_result.set_value(publication);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	future_node->subscription().send(expected);
	EXPECT_EQ(future_result_ready.get(), expected);
}

TEST(TestFutureNode, Callback4)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};
	const std::int32_t expected{100500};
	const auto current_executed_node_publisher = make_self_shared<PublishManyForMany<CurrentExecutedNode>>();
	auto future_node = hi::FutureNode<std::int32_t, std::int32_t>::create(
		[&](std::int32_t param, hi::IPublisher<std::int32_t> & result_publisher, INode & node)
		{
			result_publisher.publish(param);
			node.publish_progress_state(true, 55);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_,
		__FILE__,
		__LINE__,
		current_executed_node_publisher,
		12345);

	std::promise<std::int32_t> future_result;
	auto future_result_ready = future_result.get_future();

	std::promise<CurrentExecutedNode> progress_result;
	auto progress_result_ready = progress_result.get_future();

	hi::subscribe(
		*future_node->result_channel(),
		[&](std::int32_t publication) mutable
		{
			future_result.set_value(publication);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	hi::subscribe(
		*current_executed_node_publisher->subscribe_channel(),
		[&](CurrentExecutedNode current_executed_node)
		{
			progress_result.set_value(current_executed_node);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	future_node->subscription().send(expected);
	EXPECT_EQ(future_result_ready.get(), expected);
	const auto & current_executed_node = progress_result_ready.get();
	EXPECT_EQ(current_executed_node.node_id_, 12345);
	EXPECT_EQ(current_executed_node.achieved_progress_, 55);
	EXPECT_TRUE(current_executed_node.in_progress_);
}

TEST(TestFutureNode, AfterFutureNodeDeleted)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};

	auto publisher = hi::make_self_shared<PublishOneForMany<std::uint32_t>>();

	std::atomic<std::uint32_t> subscriber1_signal{0};
	std::atomic<std::uint32_t> subscriber2_signal{0};

	auto future_node1 = hi::FutureNode<std::uint32_t, std::uint32_t>::create(
		[&](std::uint32_t publication)
		{
			subscriber1_signal = publication;
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	{ // scope

		auto future_node2 = hi::FutureNode<std::uint32_t, std::uint32_t>::create(
			[&](std::uint32_t publication)
			{
				subscriber2_signal = publication;
			},
			highway.object_->protector_for_tests_only(),
			highway.object_);

		publisher->subscribe_channel()->subscribe(future_node1->subscription());
		publisher->subscribe_channel()->subscribe(future_node2->subscription());

		for (std::uint32_t signal = 1; signal < 10; ++signal)
		{
			publisher->publish(signal);
		}
		highway.object_->flush_tasks();

		EXPECT_GT(subscriber2_signal, 0);
		EXPECT_EQ(subscriber2_signal, subscriber1_signal);
	} // scope

	for (std::uint32_t signal = 10; signal < 20; ++signal)
	{
		publisher->publish(signal);
	}
	highway.object_->flush_tasks();

	EXPECT_GT(subscriber1_signal, 10);
	EXPECT_LT(subscriber2_signal, 10);
}

TEST(TestFutureNode, Protector)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};

	auto publisher = hi::make_self_shared<PublishOneForMany<std::uint32_t>>();

	std::atomic<std::uint32_t> subscriber1_signal{0};
	std::atomic<std::uint32_t> subscriber2_signal{0};

	auto protector1 = std::make_shared<bool>();
	auto protector2 = std::make_shared<bool>();

	auto future_node1 = hi::FutureNode<std::uint32_t, std::uint32_t>::create(
		[&](std::uint32_t publication)
		{
			subscriber1_signal = publication;
		},
		std::weak_ptr<bool>(protector1),
		highway.object_);

	auto future_node2 = hi::FutureNode<std::uint32_t, std::uint32_t>::create(
		[&](std::uint32_t publication)
		{
			subscriber2_signal = publication;
		},
		std::weak_ptr<bool>(protector2),
		highway.object_);

	publisher->subscribe_channel()->subscribe(future_node1->subscription());
	publisher->subscribe_channel()->subscribe(future_node2->subscription());

	for (std::uint32_t signal = 1; signal < 10; ++signal)
	{
		publisher->publish(signal);
	}
	highway.object_->flush_tasks();

	EXPECT_GT(subscriber2_signal, 0);
	EXPECT_EQ(subscriber2_signal, subscriber1_signal);

	protector2.reset();

	for (std::uint32_t signal = 10; signal < 20; ++signal)
	{
		publisher->publish(signal);
	}
	highway.object_->flush_tasks();

	EXPECT_GT(subscriber1_signal, 10);
	EXPECT_LT(subscriber2_signal, 10);
}

} // namespace hi
