/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

/*
 Тест отключения/подключения подписчика:
*/
TEST(TestConnectionsNotifier, OnFirstConnectedOnLastDisconnected)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};

	std::atomic_int first_connected_counter{0};
	std::atomic_int last_disconnected_counter{0};
	std::atomic_int received_counter{0};

	const auto publisher = hi::make_self_shared<hi::HighWayStickyPublisherWithConnectionsNotifier<std::uint32_t>>(
		[&]
		{
			++first_connected_counter;
		},
		[&]
		{
			++last_disconnected_counter;
		},
		highway.object_);

	auto subscribe_channel = publisher->subscribe_channel();

	auto subscription1 = subscribe_channel->subscribe(
		[&](std::uint32_t /* publication */)
		{
			++received_counter;
		},
		false);
	{
		auto subscription2 = subscribe_channel->subscribe(
			[&](std::uint32_t /* publication */)
			{
				++received_counter;
			},
			false);

		auto subscription3 = subscribe_channel->subscribe(
			[&](std::uint32_t /* publication */)
			{
				++received_counter;
			},
			false);

		publisher->publish(12345);
		highway->flush_tasks();
		EXPECT_EQ(first_connected_counter, 1);
		EXPECT_EQ(last_disconnected_counter, 0);
		EXPECT_EQ(received_counter, 3);
	}
	publisher->publish(12345);
	highway->flush_tasks();

	EXPECT_EQ(first_connected_counter, 1);
	EXPECT_EQ(last_disconnected_counter, 0);
	EXPECT_EQ(received_counter, 4);

	subscription1.reset();
	publisher->publish(12345);
	highway->flush_tasks();

	EXPECT_EQ(first_connected_counter, 1);
	EXPECT_EQ(last_disconnected_counter, 1);
	EXPECT_EQ(received_counter, 4);
}
