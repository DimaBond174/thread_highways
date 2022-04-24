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
	PublishManyForManyCanUnSubscribe<std::uint32_t>,
	PublishManyForMany<std::uint32_t>,
	PublishOneForMany<std::string>,
	PublishManyForManyCanUnSubscribe<std::string>,
	PublishManyForMany<std::string>>;

template <class T>
struct TestBufferedRetransmitter : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestBufferedRetransmitter, publisher_types);

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
 Тест простой доставки публикации подписчикам:
 - без отправки буфферизированного значения новым подписчикам (resend_to_just_connected = false)
 - без фильтрации одинаковых значений (send_new_only = false)
*/
TYPED_TEST(TestBufferedRetransmitter, BufferedSend)
{
	std::string result;
	std::mutex result_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 3, 3, 4, 5, 5, 5, 5, 5, 6, 7};
	const std::string expected_result{"1233345555567"};

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto publisher = hi::make_self_shared<TypeParam>();
	auto buffered_retransmitter =
		hi::make_self_shared<hi::BufferedRetransmitter<typename TypeParam::PublicationType, false, false>>(
			publisher->subscribe_channel(),
			to_target_type<std::uint32_t, typename TypeParam::PublicationType>(777));

	hi::subscribe(
		buffered_retransmitter->subscribe_channel(),
		highway,
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result_protector};
			result.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		});

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

/*
 Тест доставки публикации подписчикам:
 - c отправкой буфферизированного значения новым подписчикам (resend_to_just_connected = true)
 - без фильтрации одинаковых значений (send_new_only = false)
*/
TYPED_TEST(TestBufferedRetransmitter, BufferedSendResendToJustConnected)
{
	std::string result;
	std::mutex result_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 3, 3, 4, 5, 5, 5, 5, 5, 6, 7};
	const std::string expected_result{"7771233345555567"};

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto publisher = hi::make_self_shared<TypeParam>();
	auto buffered_retransmitter =
		hi::make_self_shared<hi::BufferedRetransmitter<typename TypeParam::PublicationType, true, false>>(
			publisher->subscribe_channel(),
			to_target_type<std::uint32_t, typename TypeParam::PublicationType>(777));

	hi::subscribe(
		buffered_retransmitter->subscribe_channel(),
		highway,
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result_protector};
			result.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		});

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

/*
 Тест доставки публикации подписчикам:
 - без отправки буфферизированного значения новым подписчикам (resend_to_just_connected = false)
 - с фильтрацией одинаковых значений (send_new_only = true)
*/
TYPED_TEST(TestBufferedRetransmitter, BufferedSendNewOnly)
{
	std::string result;
	std::mutex result_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 3, 3, 4, 5, 5, 5, 5, 5, 6, 7};
	const std::string expected_result{"1234567"};

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto publisher = hi::make_self_shared<TypeParam>();
	auto buffered_retransmitter =
		hi::make_self_shared<hi::BufferedRetransmitter<typename TypeParam::PublicationType, false, true>>(
			publisher->subscribe_channel(),
			to_target_type<std::uint32_t, typename TypeParam::PublicationType>(777));

	hi::subscribe(
		buffered_retransmitter->subscribe_channel(),
		highway,
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result_protector};
			result.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		});

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

/*
 Тест доставки публикации подписчикам:
 - c отправкой буфферизированного значения новым подписчикам (resend_to_just_connected = true)
 - c фильтрацией одинаковых значений (send_new_only = true)
*/
TYPED_TEST(TestBufferedRetransmitter, BufferedSendResendToJustConnectedNewOnly)
{
	std::string result;
	std::mutex result_protector;
	std::vector<std::uint32_t> data{1, 2, 3, 3, 3, 4, 5, 5, 5, 5, 5, 6, 7};
	const std::string expected_result{"7771234567"};

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	auto publisher = hi::make_self_shared<TypeParam>();
	auto buffered_retransmitter =
		hi::make_self_shared<hi::BufferedRetransmitter<typename TypeParam::PublicationType, true, true>>(
			publisher->subscribe_channel(),
			to_target_type<std::uint32_t, typename TypeParam::PublicationType>(777));

	hi::subscribe(
		buffered_retransmitter->subscribe_channel(),
		highway,
		[&](typename TypeParam::PublicationType message)
		{
			std::lock_guard lg{result_protector};
			result.append(to_target_type<typename TypeParam::PublicationType, std::string>(message));
		});

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
