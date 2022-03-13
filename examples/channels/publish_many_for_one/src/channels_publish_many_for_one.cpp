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

	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
		[](std::string publication)
		{
			std::cout << "test_1, publication = " << publication << std::endl;
		},
		highway->protector(),
		__FILE__,
		__LINE__);

	hi::PublishManyForOne<std::string> publisher{
		hi::Subscription<std::string>::create(std::move(subscription_callback), highway->mailbox())};

	std::thread thread1{[&]
						{
							for (int i = 0; i <= 100; ++i)
							{
								publisher.publish(std::string("thread1:").append(std::to_string(i)));
							}
						}};
	std::thread thread2{[&]
						{
							for (int i = 0; i <= 100; ++i)
							{
								publisher.publish(std::string("thread2:").append(std::to_string(i)));
							}
						}};
	std::thread thread3{[&]
						{
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
