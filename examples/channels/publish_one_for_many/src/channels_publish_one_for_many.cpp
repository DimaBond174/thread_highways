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
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	auto highway_mailbox = highway->mailbox();
	auto subscription_callback1 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication)
		{
			std::cout << "test_1, publication = " << publication << std::endl;
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback1), highway_mailbox));
	hi::ISubscribeHerePtr<std::string> channel;
	channel->subscribe(hi::Subscription<std::string>::create(std::move(subscription_callback1), highway_mailbox));
	auto subscription_callback2 = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication)
		{
			std::cout << "test_2, publication = " << publication << std::endl;
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback2), highway_mailbox));
	publisher->publish(std::string{"Mother washed the frame"});

	highway->flush_tasks();
} // test_1

int main(int /* argc */, char ** /* argv */)
{
	test_1();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
