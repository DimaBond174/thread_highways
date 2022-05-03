#include <thread_highways/include_all.h>

#include <cassert>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

void test_1()
{
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	highway->mailbox()->send_may_blocked(hi::Runnable::create(
		[](const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			hi::set_this_thread_name("highway");
		},
		__FILE__,
		__LINE__));

	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			std::cout << "test_1, publication = " << publication << std::endl;
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);

	hi::PublishManyForOne<std::string> publisher{
		hi::Subscription<std::string>::create(std::move(subscription_callback), highway->mailbox())};

	std::thread thread1{[&]
						{
							hi::set_this_thread_name("thread1");
							for (int i = 0; i <= 100; ++i)
							{
								publisher.publish(std::string("thread1:").append(std::to_string(i)));
							}
						}};
	std::thread thread2{[&]
						{
							hi::set_this_thread_name("thread2");
							for (int i = 0; i <= 100; ++i)
							{
								publisher.publish(std::string("thread2:").append(std::to_string(i)));
							}
						}};
	std::thread thread3{[&]
						{
							hi::set_this_thread_name("thread3");
							for (int i = 0; i <= 100; ++i)
							{
								publisher.publish(std::string("thread3:").append(std::to_string(i)));
							}
						}};

	publisher.publish(std::string{"Mother washed the frame"});

	thread1.join();
	thread2.join();
	thread3.join();

	highway->flush_tasks();
} // test_1

int main(int /* argc */, char ** /* argv */)
{
	test_1();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
