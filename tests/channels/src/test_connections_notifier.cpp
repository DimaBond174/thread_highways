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
	PublishOneForManyWithConnectionsNotifier<std::uint32_t>,
	PublishManyForManyWithConnectionsNotifier<std::uint32_t>,
	PublishOneForManyWithConnectionsNotifier<std::string>,
	PublishManyForManyWithConnectionsNotifier<std::string>>;

template <class T>
struct TestConnectionsNotifier : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestConnectionsNotifier, publisher_types);

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
 Тест отключения/подключения подписчика:
*/
TYPED_TEST(TestConnectionsNotifier, OnFirstOnLast)
{
	auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();

	std::atomic_bool was_first_connected{false};
	std::atomic_bool was_last_disconnected{false};

	auto publisher = hi::make_self_shared<TypeParam>(
		[&]
		{
			was_first_connected = true;
		},
		[&]
		{
			was_last_disconnected = true;
		});

	using PublicationType = typename TypeParam::PublicationType;
	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			ISubscribeHere<PublicationType> & channel,
			IHighWayMailBoxPtr highway_mailbox)
			: future_{promise_.get_future()}
		{
			hi::subscribe(
				channel,
				[this](PublicationType result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					promise_.set_value(result);
				},
				self_weak,
				highway_mailbox);
		}

		std::promise<PublicationType> promise_;
		std::future<PublicationType> future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	const PublicationType expected{to_target_type<std::uint32_t, PublicationType>(777)};

	for (int i = 0; i < 10; ++i)
	{
		{
			auto subscriber =
				hi::make_self_shared<SelfProtectedChecker>(*publisher->subscribe_channel(), highway->mailbox());
			EXPECT_TRUE(was_first_connected);
			EXPECT_FALSE(was_last_disconnected);

			was_first_connected = false;
			publisher->publish(expected);
			EXPECT_EQ(expected, subscriber->future_.get());
			EXPECT_FALSE(was_first_connected);
			EXPECT_FALSE(was_last_disconnected);
		}

		// Detection of disconnected subscribers occurs at the time of distribution of the publication
		publisher->publish(expected);

		// Wait until publication will be received by subscriber on highway
		highway->flush_tasks();

		EXPECT_FALSE(was_first_connected);
		EXPECT_TRUE(was_last_disconnected);
		was_last_disconnected = false;
	}

	highway->destroy();
}

TYPED_TEST(TestConnectionsNotifier, ShouldCallCallbackOnlyOnceForSeveralConnections)
{
	auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();
	auto mailbox = highway->mailbox();

	std::atomic_int first_connected_counter{0};
	std::atomic_int last_disconnected_counter{0};

	auto publisher = hi::make_self_shared<TypeParam>(
		[&]
		{
			++first_connected_counter;
		},
		[&]
		{
			++last_disconnected_counter;
		});

	using PublicationType = typename TypeParam::PublicationType;

	auto fake_protector = std::make_shared<bool>();

	publisher->subscribe([](PublicationType) {}, std::weak_ptr(fake_protector), mailbox);
	publisher->subscribe([](PublicationType) {}, std::weak_ptr(fake_protector), mailbox);
	publisher->subscribe([](PublicationType) {}, std::weak_ptr(fake_protector), mailbox);
	EXPECT_EQ(1, first_connected_counter);
	EXPECT_EQ(0, last_disconnected_counter);

	fake_protector.reset();

	// Detection of disconnected subscribers occurs at the time of distribution of the publication
	publisher->publish(PublicationType{});

	// Wait until publication will be received by subscriber on highway
	highway->flush_tasks();

	EXPECT_EQ(1, first_connected_counter);
	EXPECT_EQ(1, last_disconnected_counter);

	highway->destroy();
}

} // namespace
} // namespace hi
