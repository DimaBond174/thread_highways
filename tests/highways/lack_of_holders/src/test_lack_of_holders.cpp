/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <type_traits>

namespace hi
{

using highway_types = ::testing::Types<
	SerialHighWay<>,
	SerialHighWayDebug<>,
	SerialHighWayWithScheduler<>,
	SingleThreadHighWay<>,
	SingleThreadHighWayWithScheduler<>,
	ConcurrentHighWay<>,
	ConcurrentHighWayDebug<>>;

template <class T>
struct TestLackOfHolders : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestLackOfHolders, highway_types);

/*
 Тест нехватки холдеров для передачи сообщений.
 Два хайвея шлют друг другу сообщения, количество сообщений, которые могут быть в работе, ограничено.

 Один из хайвеев обрабатывает сообщенния намного дольше,
 поэтому если общее количество сообщений не ограничивать через доступные холдеры
 то более быстрый хайвей будет генерировать объекты сообщений и рост их количества может
 съесть всю доступную память. Чтобы этого не произошло устанавливается ограничение на
 общее количество холдеров сообщений - но это может приводить к тому что более быстрый поток
 либо будет игнорировать невозможность отправки, либо вставать на ожидание освобождения холдера.

 В этом тесте проверяется что будет происходить без системы монитринга (которая могла бы перезапустить хайвеи).
*/
TYPED_TEST(TestLackOfHolders, TwoHighwaysWithRunnables)
{
	// Будем проверять что сообщение прошло через хайвеи N раз
	std::atomic<std::uint32_t> message1_loop_id{0};
	std::atomic<std::uint32_t> message2_loop_id{0};
	struct Message
	{
		Message(std::atomic<std::uint32_t> & message_loop_id)
			: message_loop_id_{message_loop_id}
		{
		}
		std::atomic<std::uint32_t> & message_loop_id_;
	};

	auto message1 = std::make_unique<Message>(message1_loop_id);
	auto message2 = std::make_unique<Message>(message2_loop_id);

	message1->message_loop_id_++;
	message2->message_loop_id_++;

	EXPECT_EQ(1, message1_loop_id);
	EXPECT_EQ(1, message2_loop_id);

	auto highway1 = hi::make_self_shared<TypeParam>();
	auto highway2 = hi::make_self_shared<TypeParam>();

	/*
	 * На каждый хайвей будет запущен Runnable который пошлёт сообщение на другой хайвей,
	 * значит минимально в моменте времени хайвей должен суметь обработать
	 * 1 Runnable + 1 Message = 2 объекта
	 */
	highway1->set_capacity(2);
	highway2->set_capacity(2);

	auto highway1_mailbox = highway1->mailbox();
	auto highway2_mailbox = highway2->mailbox();

	// must run on highway1
	std::function<void(std::unique_ptr<Message> &&)> resender1;
	// must run on highway2
	std::function<void(std::unique_ptr<Message> &&)> resender2;

	resender1 = [&](std::unique_ptr<Message> && message)
	{
		EXPECT_TRUE(highway1->current_execution_on_this_highway());
		message->message_loop_id_++;
		highway2_mailbox->send_may_blocked(hi::Runnable::create(
			[&, message = std::move(message)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				resender2(std::move(message));
			},
			__FILE__,
			__LINE__));
	};

	resender2 = [&](std::unique_ptr<Message> && message)
	{
		EXPECT_TRUE(highway2->current_execution_on_this_highway());
		message->message_loop_id_++;
		highway1_mailbox->send_may_blocked(hi::Runnable::create(
			[&, message = std::move(message)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				// slow worker
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				resender1(std::move(message));
			},
			__FILE__,
			__LINE__));
	};

	// Запускаем цикл
	highway1_mailbox->send_may_blocked(hi::Runnable::create(
		[&, message = std::move(message1)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
		{
			resender1(std::move(message));
		},
		__FILE__,
		__LINE__));

	highway2_mailbox->send_may_blocked(hi::Runnable::create(
		[&, message = std::move(message2)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
		{
			resender2(std::move(message));
		},
		__FILE__,
		__LINE__));

	// Если произойдёт дедлок, то это ожидание будет вечным
	std::uint32_t expected_count{100};
	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	while (message1_loop_id < expected_count && message2_loop_id < expected_count);

	highway1->destroy();
	highway2->destroy();
}

TYPED_TEST(TestLackOfHolders, TwoHighwaysWithPublishers)
{
	// Будем проверять что сообщение прошло через хайвеи N раз
	std::atomic<std::uint32_t> message1_loop_id{0};
	std::atomic<std::uint32_t> message2_loop_id{0};
	struct Message
	{
		Message(std::atomic<std::uint32_t> & message_loop_id)
			: message_loop_id_{message_loop_id}
		{
		}
		std::atomic<std::uint32_t> & message_loop_id_;
	};

	auto publisher1 = hi::make_self_shared<hi::PublishOneForMany<std::shared_ptr<Message>>>();
	auto publisher2 = hi::make_self_shared<hi::PublishOneForMany<std::shared_ptr<Message>>>();

	// в этом случае уже не можем использовать std::unique_ptr
	// так как публикация доставляется нескольким подписчикам (копируется)
	auto message1 = std::make_shared<Message>(message1_loop_id);
	auto message2 = std::make_shared<Message>(message2_loop_id);

	message1->message_loop_id_++;
	message2->message_loop_id_++;

	EXPECT_EQ(1, message1_loop_id);
	EXPECT_EQ(1, message2_loop_id);

	auto highway1 = hi::make_self_shared<TypeParam>();
	auto highway2 = hi::make_self_shared<TypeParam>();

	/*
	 * На каждый хайвей будет запущен Runnable который пошлёт сообщение на другой хайвей,
	 * значит минимально в моменте времени хайвей должен суметь обработать
	 * 1 Runnable + 1 Message = 2 объекта
	 */
	highway1->set_capacity(2);
	highway2->set_capacity(2);

	publisher1->subscribe(
		[&](std::shared_ptr<Message> message)
		{
			EXPECT_TRUE(highway2->current_execution_on_this_highway());
			message->message_loop_id_++;
			publisher2->publish(std::move(message));
		},
		highway2->protector_for_tests_only(),
		highway2->mailbox(),
		false,
		__FILE__,
		__LINE__);

	publisher2->subscribe(
		[&](std::shared_ptr<Message> message)
		{
			EXPECT_TRUE(highway1->current_execution_on_this_highway());
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
			message->message_loop_id_++;
			publisher1->publish(std::move(message));
		},
		highway1->protector_for_tests_only(),
		highway1->mailbox(),
		false,
		__FILE__,
		__LINE__);

	// Запускаем цикл
	hi::execute(
		[&]()
		{
			publisher1->publish(std::move(message1));
		},
		highway1);
	hi::execute(
		[&]
		{
			publisher2->publish(std::move(message2));
		},
		highway2);

	// Если произойдёт дедлок, то это ожидание будет вечным
	std::uint32_t expected_count{100};
	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	while (message1_loop_id < expected_count && message2_loop_id < expected_count);

	highway1->destroy();
	highway2->destroy();
}

} // namespace hi
