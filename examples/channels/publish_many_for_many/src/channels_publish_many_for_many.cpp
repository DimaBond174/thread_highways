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
	auto publisher = hi::make_self_shared<hi::PublishManyForMany<std::string>>();
	auto highway_mailbox = highway->mailbox();

	auto subscription_callback1 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			std::cout << "test_1, publication = " << publication << std::endl;
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback1), highway_mailbox));

	auto subscription_callback2 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			std::cout << "test_2, publication = " << publication << std::endl;
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback2), highway_mailbox));

	std::thread thread1{[&]
						{
							for (int i = 0; i <= 100; ++i)
							{
								publisher->publish(std::string("thread1:").append(std::to_string(i)));
							}
						}};
	std::thread thread2{[&]
						{
							for (int i = 0; i <= 100; ++i)
							{
								publisher->publish(std::string("thread2:").append(std::to_string(i)));
							}
						}};
	std::thread thread3{[&]
						{
							for (int i = 0; i <= 100; ++i)
							{
								publisher->publish(std::string("thread3:").append(std::to_string(i)));
							}
						}};

	publisher->publish(std::string{"Mother washed the frame"});

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
