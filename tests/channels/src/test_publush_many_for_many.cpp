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
using publisher_types =
	::testing::Types<PublishManyForManyCanUnSubscribe<std::uint32_t>, PublishManyForMany<std::uint32_t>>;

template <class T>
struct TestPublushManyForMany : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestPublushManyForMany, publisher_types);

/*
 Тест доставки публикации подписчикам.
*/
TYPED_TEST(TestPublushManyForMany, DirectSend)
{
	std::atomic<std::uint32_t> result1{0};
	std::atomic<std::uint32_t> result2{0};
	std::vector<std::uint32_t> data1{1, 2, 3, 4, 5, 6, 7};
	std::vector<std::uint32_t> data2{8, 9, 10, 11, 12, 13, 14};
	const std::uint32_t expected_result =
		std::accumulate(data1.begin(), data1.end(), 0) + std::accumulate(data2.begin(), data2.end(), 0);

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto publisher = hi::make_self_shared<TypeParam>();

	hi::subscribe(
		*publisher->subscribe_channel(),
		[&](typename TypeParam::PublicationType message)
		{
			result1 += message;
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

	hi::subscribe(
		*publisher->subscribe_channel(),
		[&](typename TypeParam::PublicationType message)
		{
			result2 += message;
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

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

	EXPECT_EQ(expected_result, result1);
	EXPECT_EQ(expected_result, result2);

	highway->destroy();
}

} // namespace
} // namespace hi
