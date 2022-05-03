#include <thread_highways/include_all.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

using namespace std::chrono_literals;

void increase_decrease_workers()
{
	auto logger = hi::create_default_logger(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});

	auto highway = hi::make_self_shared<hi::ConcurrentHighWayDebug<>>(
		"ConcurrentHighWayDebug:increase_decrease_workers",
		std::move(logger),
		std::chrono::milliseconds{10},
		std::chrono::milliseconds{100});

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t i, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			std::this_thread::sleep_for(10ms);
			std::cout << "\nmessage:" << i << std::endl;
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	// will increase_workers()
	for (std::int32_t i = 0; i < 100; ++i)
	{
		publisher->publish(i);
		if (i % 10 == 0)
		{
			std::this_thread::sleep_for(100ms);
		}
	}

	// will decrease_workers()
	std::this_thread::sleep_for(5000ms);
	// will increase_workers()
	for (std::int32_t i = 0; i < 100; ++i)
	{
		publisher->publish(i);
		if (i % 10 == 0)
		{
			std::this_thread::sleep_for(100ms);
		}
	}
	highway->flush_tasks();
	highway->destroy();
} // monitor_and_repair_hungs_concurrent_highway

int main(int /* argc */, char ** /* argv */)
{
	increase_decrease_workers();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
