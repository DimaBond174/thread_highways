/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

namespace hi
{
namespace
{

TEST(TestSubscriptionCallbacks, DirectSend)
{
	std::promise<bool> promise;
	auto future = promise.get_future();

	const auto check = [&]
	{
		EXPECT_TRUE(future.get());
		promise = {};
		future = promise.get_future();
	};

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<int>>();
	const auto channel = publisher->subscribe_channel();

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&]()
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector));
		publisher->publish(0);
		check();
	}

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&](int)
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector));
		publisher->publish(0);
		check();
	}
}

TEST(TestSubscriptionCallbacks, SendOnHighWay)
{
	std::promise<bool> promise;
	auto future = promise.get_future();

	const auto check = [&]
	{
		EXPECT_TRUE(future.get());
		promise = {};
		future = promise.get_future();
	};

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<int>>();
	const auto channel = publisher->subscribe_channel();

	const auto highway = hi::make_self_shared<SerialHighWay<>>();
	const auto mailbox = highway->mailbox();

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&]()
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector));
		publisher->publish(0);
		check();
	}

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&](int)
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector),
			mailbox);
		publisher->publish(0);
		check();
	}

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&](int, const std::atomic<std::uint32_t> &, const std::uint32_t)
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector),
			mailbox);
		publisher->publish(0);
		check();
	}

	{
		const auto protector = std::make_shared<bool>();
		hi::subscribe(
			channel,
			[&](SubscriptionCallback<int>::LaunchParameters)
			{
				promise.set_value(true);
			},
			std::weak_ptr(protector),
			mailbox);
		publisher->publish(0);
		check();
	}
	highway->destroy();
}

} // namespace
} // namespace hi
