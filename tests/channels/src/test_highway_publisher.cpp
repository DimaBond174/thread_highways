/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <vector>

TEST(TestPublushOneForOne, SendOnHighway)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto publisher = hi::make_self_shared<hi::HighWayPublisher<std::uint32_t>>(*highway);
	auto channel = publisher->subscribe_channel();

	const std::vector<std::uint32_t> all_data{1, 2, 3, 4, 4, 4, 5, 6, 7, 7, 7};
	const std::vector<std::uint32_t> new_only_data{1, 2, 3, 4, 5, 6, 7};

	std::vector<std::uint32_t> res1;
	auto subscr1 = channel->subscribe(
		[&](std::uint32_t publication)
		{
			res1.push_back(publication);
		},
		false /* for_new_only */);

	std::vector<std::uint32_t> res2;
	auto subscr2 = channel->subscribe(
		[&](std::uint32_t publication)
		{
			res2.push_back(publication);
		},
		true /* for_new_only */);

	for (auto it : all_data)
	{
		publisher->publish(it);
	}

	highway->flush_tasks();

	EXPECT_EQ(res1, all_data);
	EXPECT_EQ(res2, new_only_data);
}
