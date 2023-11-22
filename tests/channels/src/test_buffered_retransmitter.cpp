/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace hi
{
namespace
{

TEST(ControlTransmittedData, StickyPublisherShouldPassTheLastMessageToNewSubscriptions)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	auto publisher = hi::make_self_shared<hi::HighWayStickyPublisher<std::uint32_t>>(highway.object_);
	auto subscribe_channel = publisher->subscribe_channel();
	std::atomic<std::uint32_t> res{0};

	auto subscription = subscribe_channel->subscribe(
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		false);

	highway.object_->flush_tasks();
	EXPECT_EQ(res, 0);

	publisher->publish(12345);
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 12345);

	res = 0;
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 0);

	// New subscriber
	auto subscription2 = subscribe_channel->subscribe(
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		false);

	// Should receive last publication
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 12345);
}

TEST(ControlTransmittedData, SubscriptionShouldBeAbleToFilterDuplicates)
{
	std::uint32_t res{0};
	hi::PublishManyForOne<std::uint32_t> publisher{
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		true // for_new_only
	};

	publisher.publish(12345);
	EXPECT_EQ(res, 12345);

	res = 0;
	publisher.publish(12345);
	EXPECT_EQ(res, 0);
}

TEST(ControlTransmittedData, SubscriptionShouldBeAbleToNotFilterDuplicates)
{
	std::uint32_t res{0};
	hi::PublishManyForOne<std::uint32_t> publisher{
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		false // for_new_only
	};

	publisher.publish(12345);
	EXPECT_EQ(res, 12345);

	res = 0;
	publisher.publish(12345);
	EXPECT_EQ(res, 12345);
}

TEST(ControlTransmittedData, HighWaySubscriptionShouldBeAbleToFilterDuplicates)
{
	std::atomic<std::uint32_t> res{0};

	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	hi::PublishManyForOne<std::uint32_t> publisher{
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		highway.object_,
		__FILE__,
		__LINE__,
		false,
		true // for_new_only
	};

	publisher.publish(12345);
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 12345);

	res = 0;
	publisher.publish(12345);
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 0);
}

TEST(ControlTransmittedData, HighWaySubscriptionShouldBeAbleToNotFilterDuplicates)
{
	std::atomic<std::uint32_t> res{0};

	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	hi::PublishManyForOne<std::uint32_t> publisher{
		[&](std::uint32_t publication)
		{
			res = publication;
		},
		highway.object_,
		__FILE__,
		__LINE__,
		false,
		false // for_new_only
	};

	publisher.publish(12345);
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 12345);

	res = 0;
	publisher.publish(12345);
	highway.object_->flush_tasks();
	EXPECT_EQ(res, 12345);
}

} // namespace
} // namespace hi
