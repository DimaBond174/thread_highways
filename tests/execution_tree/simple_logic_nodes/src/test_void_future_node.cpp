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

TEST(TestVoidFutureNode, MonitoringDifferentSources)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};

	auto source1 = hi::VoidFutureNode<std::string>::create(
		[](hi::IPublisher<std::string> & result_publisher)
		{
			result_publisher.publish("Mother washed the frame");
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	auto source2 = hi::VoidFutureNode<std::uint32_t>::create(
		[](hi::IPublisher<std::uint32_t> & result_publisher)
		{
			static std::atomic<std::uint32_t> frames_count{0};
			result_publisher.publish(++frames_count);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	std::promise<bool> future_result;
	auto future_result_ready = future_result.get_future();

	auto future_node = hi::VoidFutureNode<bool>::create(
		[&]()
		{
			static std::atomic<std::uint32_t> signals{0};
			++signals;
			if (signals >= 100)
			{
				future_result.set_value(true);
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	// Subscribe to publications of different types: std::string, std::uint32_t
	source1->result_channel()->subscribe(future_node->subscription<std::string>());
	source2->result_channel()->subscribe(future_node->subscription<std::uint32_t>());

	for (std::uint32_t i = 0; i < 50; ++i)
	{
		source1->execute();
		source2->execute();
	}

	EXPECT_EQ(future_result_ready.get(), true);
}

TEST(TestVoidFutureNode, Callback1)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};
	const std::int32_t expected{100500};
	auto future_node = hi::VoidFutureNode<std::int32_t>::create(
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

TEST(TestVoidFutureNode, Callback3)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};
	const std::int32_t expected{100500};
	auto future_node = hi::VoidFutureNode<std::int32_t>::create(
		[&](hi::IPublisher<std::int32_t> & result_publisher,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id)
		{
			EXPECT_EQ(global_run_id, your_run_id);
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

TEST(TestVoidFutureNode, Callback4)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};
	const std::int32_t expected{100500};
	const auto current_executed_node_publisher = make_self_shared<PublishManyForMany<CurrentExecutedNode>>();
	auto future_node = hi::VoidFutureNode<std::int32_t>::create(
		[&](hi::IPublisher<std::int32_t> & result_publisher, INode & node)
		{
			result_publisher.publish(expected);
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

	current_executed_node_publisher->subscribe(
		[&](CurrentExecutedNode current_executed_node)
		{
			progress_result.set_value(current_executed_node);
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	future_node->execute();
	EXPECT_EQ(future_result_ready.get(), expected);
	const auto & current_executed_node = progress_result_ready.get();
	EXPECT_EQ(current_executed_node.node_id_, 12345);
	EXPECT_EQ(current_executed_node.achieved_progress_, 55);
	EXPECT_TRUE(current_executed_node.in_progress_);
}

TEST(TestVoidFutureNode, AfterFutureNodeDeleted)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};

	auto publisher = hi::make_self_shared<PublishOneForMany<std::uint32_t>>();

	std::atomic<std::uint32_t> subscriber1_signal{0};
	std::atomic<std::uint32_t> subscriber2_signal{0};

	auto future_node1 = hi::VoidFutureNode<std::uint32_t>::create(
		[&]()
		{
			++subscriber1_signal;
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	{ // scope

		auto future_node2 = hi::VoidFutureNode<std::uint32_t>::create(
			[&]()
			{
				++subscriber2_signal;
			},
			highway.object_->protector_for_tests_only(),
			highway.object_);

		publisher->subscribe_channel()->subscribe(future_node1->subscription<std::uint32_t>());
		publisher->subscribe_channel()->subscribe(future_node2->subscription<std::uint32_t>());

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

TEST(TestVoidFutureNode, Protector)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};

	auto publisher = hi::make_self_shared<PublishOneForMany<std::uint32_t>>();

	std::atomic<std::uint32_t> subscriber1_signal{0};
	std::atomic<std::uint32_t> subscriber2_signal{0};

	auto protector1 = std::make_shared<bool>();
	auto protector2 = std::make_shared<bool>();

	auto future_node1 = hi::VoidFutureNode<std::uint32_t>::create(
		[&]()
		{
			++subscriber1_signal;
		},
		std::weak_ptr<bool>(protector1),
		highway.object_);

	auto future_node2 = hi::VoidFutureNode<std::uint32_t>::create(
		[&]()
		{
			++subscriber2_signal;
		},
		std::weak_ptr<bool>(protector2),
		highway.object_);

	publisher->subscribe_channel()->subscribe(future_node1->subscription<std::uint32_t>());
	publisher->subscribe_channel()->subscribe(future_node2->subscription<std::uint32_t>());

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
