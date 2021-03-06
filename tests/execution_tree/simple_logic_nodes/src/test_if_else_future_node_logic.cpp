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

TEST(TestIfElseFutureNodeLogic, CheckNumberIsOddOrEven)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};

	auto if_then_node = hi::IfElseFutureNode<std::uint32_t, bool, bool>::create(
		[](std::uint32_t param, IPublisher<bool> & if_result_publisher, IPublisher<bool> & else_result_publisher)
		{
			if (param % 2)
			{
				if_result_publisher.publish(true);
			}
			else
			{
				else_result_publisher.publish(true);
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			const std::shared_ptr<IfElseFutureNode<std::uint32_t, bool, bool>> & if_then_node,
			IHighWayMailBoxPtr highway_mailbox)
			: odd_future_{odd_promise_.get_future()}
			, even_future_{even_promise_.get_future()}
		{
			hi::subscribe(
				*if_then_node->if_result_channel(),
				[this](bool result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					odd_promise_.set_value(result);
				},
				self_weak,
				highway_mailbox);

			hi::subscribe(
				*if_then_node->else_result_channel(),
				[this](bool result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					even_promise_.set_value(result);
				},
				std::move(self_weak),
				std::move(highway_mailbox));
		}

		std::promise<bool> odd_promise_;
		std::future<bool> odd_future_;
		std::promise<bool> even_promise_;
		std::future<bool> even_future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	auto if_then_node_subscription = if_then_node->subscription();
	auto mailbox = highway.object_->mailbox();
	for (std::uint32_t i = 0; i < 100; ++i)
	{
		auto checker = hi::make_self_shared<SelfProtectedChecker>(if_then_node, mailbox);
		if_then_node_subscription.send(i);
		if (i % 2)
		{
			EXPECT_EQ(true, checker->odd_future_.get());
		}
		else
		{
			EXPECT_EQ(true, checker->even_future_.get());
		}
		EXPECT_EQ(1, checker->exec_counter_);
	}
}

TEST(TestIfElseFutureNodeLogic, CheckSharedNumberIsOddOrEven)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};

	auto if_then_node = hi::
		IfElseFutureNode<std::shared_ptr<std::uint32_t>, std::shared_ptr<bool>, std::shared_ptr<std::string>>::create(
			[](std::shared_ptr<std::uint32_t> param,
			   IPublisher<std::shared_ptr<bool>> & if_result_publisher,
			   IPublisher<std::shared_ptr<std::string>> & else_result_publisher)
			{
				if (*param % 2)
				{
					if_result_publisher.publish(std::make_shared<bool>(true));
				}
				else
				{
					else_result_publisher.publish(std::make_shared<std::string>("YES"));
				}
			},
			highway.object_->protector_for_tests_only(),
			highway.object_);

	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			const std::shared_ptr<
				IfElseFutureNode<std::shared_ptr<std::uint32_t>, std::shared_ptr<bool>, std::shared_ptr<std::string>>> &
				if_then_node,
			IHighWayMailBoxPtr highway_mailbox)
			: odd_future_{odd_promise_.get_future()}
			, even_future_{even_promise_.get_future()}
		{
			hi::subscribe(
				*if_then_node->if_result_channel(),
				[this](std::shared_ptr<bool> result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					odd_promise_.set_value(*result);
				},
				self_weak,
				highway_mailbox);

			hi::subscribe(
				*if_then_node->else_result_channel(),
				[this](std::shared_ptr<std::string> result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					even_promise_.set_value(*result == "YES");
				},
				std::move(self_weak),
				std::move(highway_mailbox));
		}

		std::promise<bool> odd_promise_;
		std::future<bool> odd_future_;
		std::promise<bool> even_promise_;
		std::future<bool> even_future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	auto if_then_node_subscription = if_then_node->subscription();
	auto mailbox = highway.object_->mailbox();
	for (std::uint32_t i = 0; i < 100; ++i)
	{
		auto checker = hi::make_self_shared<SelfProtectedChecker>(if_then_node, mailbox);
		auto it = std::make_shared<std::uint32_t>(i);
		*it = i;
		if_then_node_subscription.send(std::move(it));
		if (i % 2)
		{
			EXPECT_EQ(true, checker->odd_future_.get());
		}
		else
		{
			EXPECT_EQ(true, checker->even_future_.get());
		}
		EXPECT_EQ(1, checker->exec_counter_);
	}
}

} // namespace hi
