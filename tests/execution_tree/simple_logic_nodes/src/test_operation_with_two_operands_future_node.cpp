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

TEST(TestOperationWithTwoOperandsFutureNode, StringConcatenation)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};

	auto operand1_publisher = hi::make_self_shared<PublishOneForMany<std::string>>();
	auto operand2_publisher = hi::make_self_shared<PublishManyForManyCanUnSubscribe<std::string>>();

	auto future_node = hi::OperationWithTwoOperandsFutureNode<std::string, std::string, std::string>::create(
		[](std::string operand1, std::string operand2)
		{
			EXPECT_EQ("Mother washed the frame", operand1.append(operand2));
		},
		highway.object_->protector_for_tests_only(),
		highway.object_,
		*operand1_publisher->subscribe_channel(),
		*operand2_publisher->subscribe_channel());
	operand2_publisher->publish("the frame");
	operand1_publisher->publish("Mother washed ");

	highway.object_->flush_tasks();
}

TEST(TestOperationWithTwoOperandsFutureNode, MultipleCalls)
{
	RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>()};

	auto operand1_publisher = hi::make_self_shared<PublishOneForMany<std::uint32_t>>();
	auto operand2_publisher = hi::make_self_shared<PublishOneForMany<double>>();

	auto future_node = hi::OperationWithTwoOperandsFutureNode<std::uint32_t, double, std::string>::create(
		[](std::uint32_t operand1, double operand2, IPublisher<std::string> & result_publisher)
		{
			result_publisher.publish(std::to_string(operand1).append(std::to_string(operand2)));
		},
		highway.object_->protector_for_tests_only(),
		highway.object_,
		*operand1_publisher->subscribe_channel(),
		*operand2_publisher->subscribe_channel());

	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			const std::uint32_t expected1,
			const double expected2,
			ISubscribeHere<std::string> & result_channel,
			IHighWayMailBoxPtr highway_mailbox)
			: expected_{std::to_string(expected1).append(std::to_string(expected2))}
			, future_{promise_.get_future()}
		{
			hi::subscribe(
				result_channel,
				[this](std::string result)
				{
					EXPECT_EQ(result, expected_);
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					promise_.set_value(true);
				},
				std::move(self_weak),
				std::move(highway_mailbox));
		}

		const std::string expected_;
		std::promise<bool> promise_;
		std::future<bool> future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	std::uint32_t operand1{0};
	double operand2{1.23};
	bool s{false};
	auto mailbox = highway.object_->mailbox();
	while (operand1 < 10)
	{
		++operand1;
		++operand2;
		auto checker =
			hi::make_self_shared<SelfProtectedChecker>(operand1, operand2, *future_node->result_channel(), mailbox);
		s = !s;
		if (s)
		{
			operand2_publisher->publish(operand2);
			operand1_publisher->publish(operand1);
		}
		else
		{
			operand1_publisher->publish(operand1);
			operand2_publisher->publish(operand2);
		}
		EXPECT_EQ(true, checker->future_.get());
		EXPECT_EQ(1, checker->exec_counter_);
	}
}

} // namespace hi
