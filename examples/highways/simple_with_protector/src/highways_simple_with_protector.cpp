#include <thread_highways/include_all.h>

#include <cassert>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

void test_highways(
	std::shared_ptr<hi::SerialHighWay> highway1,
	std::shared_ptr<hi::SerialHighWay> highway2,
	std::shared_ptr<hi::SerialHighWay> highway3)
{
	struct Message
	{
		std::uint32_t message_id_;
		std::uint32_t loop_id_;
	};

	auto message1 = std::make_unique<Message>(Message{1, 0});
	auto message2 = std::make_unique<Message>(Message{2, 0});
	auto message3 = std::make_unique<Message>(Message{3, 0});

	auto highway1_mailbox = highway1->mailbox();
	auto highway2_mailbox = highway2->mailbox();
	auto highway3_mailbox = highway3->mailbox();

	auto protector1 = std::weak_ptr<hi::SerialHighWay>(highway1);
	auto protector2 = std::weak_ptr<hi::SerialHighWay>(highway2);
	auto protector3 = std::weak_ptr<hi::SerialHighWay>(highway3);

	// must run on highway1
	std::function<void(std::unique_ptr<Message> &&)> resender1;
	// must run on highway2
	std::function<void(std::unique_ptr<Message> &&)> resender2;
	// must run on highway3
	std::function<void(std::unique_ptr<Message> &&)> resender3;

	resender1 = [&, protector = protector2](std::unique_ptr<Message> && message)
	{
		assert(highway1->current_execution_on_this_highway());
		++message->loop_id_;
		highway2_mailbox->send_may_blocked(hi::Runnable::create(
			[&,
			 message = std::move(message),
			 protector](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				resender2(std::move(message));
			},
			protector,
			__FILE__,
			__LINE__));
	};

	resender2 = [&, protector = protector3](std::unique_ptr<Message> && message)
	{
		assert(highway2->current_execution_on_this_highway());
		++message->loop_id_;
		highway3_mailbox->send_may_blocked(hi::Runnable::create(
			[&, message = std::move(message)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				resender3(std::move(message));
			},
			protector,
			__FILE__,
			__LINE__));
	};

	resender3 = [&, protector = protector1](std::unique_ptr<Message> && message)
	{
		assert(highway3->current_execution_on_this_highway());
		++message->loop_id_;
		if (message->loop_id_ % 10 == 0)
		{
			std::cout << "message.message_id_=" << message->message_id_ << ", message.loop_id_=" << message->loop_id_
					  << std::endl;
		}

		highway1_mailbox->send_may_blocked(hi::Runnable::create(
			[&, message = std::move(message)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				resender1(std::move(message));
			},
			protector,
			__FILE__,
			__LINE__));
	};

	// ЗАПУСК
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

	highway3_mailbox->send_may_blocked(hi::Runnable::create(
		[&, message = std::move(message3)](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
		{
			resender3(std::move(message));
		},
		__FILE__,
		__LINE__));

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));

	// No need to destroy highways with->destroy()  because Runnable was protected
	//    highway1->destroy();
	//    highway2->destroy();
	//    highway3->destroy();
} // test_highways

void test_highways_with_default_main_loop_runnable()
{
	std::cout << "====================================" << std::endl;
	std::cout << "test_highways_with_custom_main_loop_runnable" << std::endl;
	std::cout << "====================================" << std::endl;
	test_highways(
		hi::make_self_shared<hi::SerialHighWay>(std::nullopt),
		hi::make_self_shared<hi::SerialHighWay>(std::nullopt),
		hi::make_self_shared<hi::SerialHighWay>(std::nullopt));
	std::cout << "-----------------------------------------------------------------------" << std::endl;
}

void test_highways_with_custom_main_loop_runnable()
{
	std::cout << "====================================" << std::endl;
	std::cout << "test_highways_with_custom_main_loop_runnable" << std::endl;
	std::cout << "====================================" << std::endl;
	test_highways(
		hi::make_self_shared<hi::SerialHighWay>(
			hi::HighWayMainLoopRunnable::create([](hi::HighWayBundle &, const std::uint32_t) {}, __FILE__, __LINE__)),
		hi::make_self_shared<hi::SerialHighWay>(
			hi::HighWayMainLoopRunnable::create([](hi::HighWayBundle &, const std::uint32_t) {}, __FILE__, __LINE__)),
		hi::make_self_shared<hi::SerialHighWay>(
			hi::HighWayMainLoopRunnable::create([](hi::HighWayBundle &, const std::uint32_t) {}, __FILE__, __LINE__)));
	std::cout << "-----------------------------------------------------------------------" << std::endl;
} // test_highways_with_custom_main_loop_runnable

int main(int /* argc */, char ** /* argv */)
{
	test_highways_with_default_main_loop_runnable();
	test_highways_with_custom_main_loop_runnable();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
