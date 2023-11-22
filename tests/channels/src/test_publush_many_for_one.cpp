/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

namespace hi
{
namespace
{

using publisher_types = ::testing::
	Types<HighWayPublisher<std::uint32_t>, HighWayStickyPublisher<std::uint32_t>, PublishManyForOne<std::uint32_t>>;

template <class T>
struct TestPublushManyForOne : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestPublushManyForOne, publisher_types);

/*
 Тест доставки публикации подписчикам.
*/
TYPED_TEST(TestPublushManyForOne, SendOnHighway)
{
	std::atomic<std::uint32_t> result{0};
	std::vector<std::uint32_t> data1{1, 2, 3, 4, 5, 6, 7};
	std::vector<std::uint32_t> data2{8, 9, 10, 11, 12, 13, 14};
	const std::uint32_t expected_result = std::accumulate(data1.begin(), data1.end(), std::uint32_t{0})
		+ std::accumulate(data2.begin(), data2.end(), std::uint32_t{0});

	hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};

	auto subscription = create_subscription<typename TypeParam::PublicationType>(
		[&](typename TypeParam::PublicationType message)
		{
			result += message;
		},
		*highway,
		__FILE__,
		__LINE__,
		false);

	auto publisher = [&]()
	{
		if constexpr (std::is_same_v<PublishManyForOne<typename TypeParam::PublicationType>, TypeParam>)
		{
			return std::make_shared<TypeParam>(subscription);
		}
		else
		{
			auto publisher = hi::make_self_shared<TypeParam>(*highway);
			publisher->subscribe(subscription);
			return publisher;
		}
	}();

	std::thread thread1{[&]
						{
							for (auto && it : data1)
							{
								publisher->publish(it);
							}
						}};

	std::thread thread2{[&]
						{
							for (auto && it : data2)
							{
								publisher->publish(it);
							}
						}};

	thread1.join();
	thread2.join();

	highway->flush_tasks();
	highway->flush_tasks();

	EXPECT_EQ(expected_result, result);

	highway->destroy();
}

} // namespace
} // namespace hi
