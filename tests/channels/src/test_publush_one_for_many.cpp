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
using publisher_types = ::testing::Types<
	PublishOneForMany<std::uint32_t>,
	PublishOneForManyWithConnectionsNotifier<std::uint32_t>,
	PublishManyForManyCanUnSubscribe<std::uint32_t>,
	PublishManyForManyWithConnectionsNotifier<std::uint32_t>,
	PublishManyForMany<std::uint32_t>,
	PublishOneForMany<std::string>,
	PublishOneForManyWithConnectionsNotifier<std::string>,
	PublishManyForManyCanUnSubscribe<std::string>,
	PublishManyForManyWithConnectionsNotifier<std::string>,
	PublishManyForMany<std::string>>;

template <class T>
struct TestPublushOneForMany : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestPublushOneForMany, publisher_types);

template <
	class From,
	class To,
	typename std::enable_if<std::is_same_v<To, std::string> && !std::is_same_v<From, std::string>, bool>::type = true>
To to_target_type(From val)
{
	return std::to_string(val);
}

template <class From, class To, typename std::enable_if<!std::is_same_v<To, std::string>, bool>::type = true>
To to_target_type(From val)
{
	return val;
}

template <
	class From,
	class To,
	typename std::enable_if<std::is_same_v<To, std::string> && std::is_same_v<From, std::string>, bool>::type = true>
To to_target_type(From val)
{
	return val;
}

/*
 Тест доставки публикации подписчикам.
*/
TYPED_TEST(TestPublushOneForMany, DirectSend)
{
	std::string result1;
	std::mutex result1_protector;
	std::string result2;
	std::mutex result2_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 4, 5, 6, 7};
	const std::string expected_result = [&, sum_string = std::string{}]() mutable
	{
		for (auto && it : data)
		{
			sum_string.append(std::to_string(it));
		}
		return sum_string;
	}();

	RAIIdestroy guard{hi::make_self_shared<hi::SerialHighWay<>>()};

	auto publisher = hi::make_self_shared<TypeParam>();

	hi::subscribe(
		*publisher->subscribe_channel(),
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result1_protector};
			result1.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		},
		guard.object_->protector_for_tests_only(),
		guard.object_->mailbox());

	hi::subscribe(
		*publisher->subscribe_channel(),
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result2_protector};
			result2.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		},
		guard.object_->protector_for_tests_only(),
		guard.object_->mailbox());

	for (auto && it : data)
	{
		publisher->publish(to_target_type<decltype(it), typename TypeParam::PublicationType>(it));
	}

	guard.object_->flush_tasks();
	{
		std::lock_guard lg{result1_protector};
		EXPECT_EQ(expected_result, result1);
	}

	{
		std::lock_guard lg{result2_protector};
		EXPECT_EQ(expected_result, result2);
	}
}

} // namespace
} // namespace hi
