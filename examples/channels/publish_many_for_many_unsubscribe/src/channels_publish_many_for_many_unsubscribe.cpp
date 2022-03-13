#include <thread_highways/include_all.h>

#include <cassert>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

void test_1()
{
	auto highway = hi::make_self_shared<hi::SerialHighWay>();
	auto publisher = hi::make_self_shared<hi::PublishManyForManyCanUnSubscribe<std::string>>();
	auto highway_mailbox = highway->mailbox();

	std::shared_ptr<bool> protector_shared = std::make_shared<bool>();

	auto subscription_callback1 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication)
		{
			std::cout << "test_1, publication = " << publication << std::endl;
		},
		std::weak_ptr<bool>{protector_shared},
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback1), highway_mailbox));

	auto subscription_callback2 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication)
		{
			std::cout << "test_2, publication = " << publication << std::endl;
		},
		std::weak_ptr<bool>{protector_shared},
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback2), highway_mailbox));

	std::thread thread1{[&]
						{
							std::uint32_t i{0};
							while (publisher->subscribers_exist())
							{
								publisher->publish(std::string("thread1:").append(std::to_string(++i)));
							}
						}};

	std::thread thread2{[&]
						{
							std::uint32_t i{0};
							while (publisher->subscribers_exist())
							{
								publisher->publish(std::string("thread2:").append(std::to_string(++i)));
							}
						}};

	std::thread thread3{[&]
						{
							std::uint32_t i{0};
							while (publisher->subscribers_exist())
							{
								publisher->publish(std::string("thread3:").append(std::to_string(++i)));
							}
						}};

	publisher->publish(std::string{"Mother washed the frame"});

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	protector_shared.reset();

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
