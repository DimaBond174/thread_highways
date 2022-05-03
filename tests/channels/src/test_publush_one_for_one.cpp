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
	PublishManyForOne<std::uint32_t>,
	PublishManyForManyCanUnSubscribe<std::uint32_t>,
	PublishManyForMany<std::uint32_t>,
	PublishOneForMany<std::string>,
	PublishManyForOne<std::string>,
	PublishManyForManyCanUnSubscribe<std::string>,
	PublishManyForMany<std::string>>;

template <class T>
struct TestPublushOneForOne : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestPublushOneForOne, publisher_types);

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
TYPED_TEST(TestPublushOneForOne, DirectSend)
{
	std::string result;
	std::mutex result_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 4, 5, 6, 7};
	const std::string expected_result = [&, sum_string = std::string{}]() mutable
	{
		for (auto && it : data)
		{
			sum_string.append(std::to_string(it));
		}
		return sum_string;
	}();

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto callback = [&](typename TypeParam::PublicationType message)
	{
		std::lock_guard lg{result_protector};
		result.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
	};

	auto publisher = [&]()
	{
		if constexpr (
			std::is_same_v<
				PublishManyForOne<std::uint32_t>,
				TypeParam> || std::is_same_v<PublishManyForOne<std::string>, TypeParam>)
		{
			return std::make_shared<TypeParam>(
				std::move(callback),
				highway->protector_for_tests_only(),
				highway->mailbox());
		}
		else
		{
			auto publisher = hi::make_self_shared<TypeParam>();
			publisher->subscribe(std::move(callback), highway->protector_for_tests_only(), highway->mailbox());
			//			publisher->subscribe(Subscription<typename TypeParam::PublicationType>::create(
			//				std::move(callback),
			//				 highway->protector_for_tests_only(),
			//				 highway->mailbox()
			//				 ));
			// hi::subscribe(*publisher->subscribe_channel(), std::move(callback), highway->protector_for_tests_only(),
			// highway->mailbox());
			return publisher;
		}
	}();

	for (auto && it : data)
	{
		publisher->publish(to_target_type<decltype(it), typename TypeParam::PublicationType>(it));
	}

	highway->flush_tasks();
	{
		std::lock_guard lg{result_protector};
		EXPECT_EQ(expected_result, result);
	}

	highway->destroy();
}

} // namespace
} // namespace hi
