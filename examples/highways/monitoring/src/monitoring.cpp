#include <thread_highways/include_all.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

using namespace std::chrono_literals;

void monitor_shortage_of_holders()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});
	auto highway = hi::make_self_shared<hi::SerialHighWay>(
		std::nullopt,
		nullptr,
		"SerialHighWay:monitor_shortage_of_holders",
		std::move(logger));
	highway->set_capacity(1);

	// hi::HighWaysMonitoring monitoring{100ms};
	// monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::uint32_t>::create(
		[&](std::uint32_t)
		{
			// std::this_thread::sleep_for(100ms);
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::uint32_t>::create(std::move(subscription_callback), highway->mailbox()));

	for (std::uint32_t i = 0; i < 1000; ++i)
	{
		publisher->publish(i);
	}

	highway->destroy();
} // monitor_shortage_of_holders()

void monitor_exception()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});
	auto highway = hi::make_self_shared<hi::SerialHighWay>(
		std::nullopt,
		nullptr,
		"SerialHighWay:monitor_exception",
		std::move(logger));

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t)
		{
			throw std::logic_error("some logic_error");
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	publisher->publish(0);

	highway->flush_tasks();
	highway->destroy();
}

void monitor_hungs()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});
	auto highway = hi::make_self_shared<hi::SerialHighWay>(
		std::nullopt,
		nullptr,
		"SerialHighWay:monitor_hungs",
		std::move(logger),
		std::chrono::milliseconds{10});

	// hi::HighWaysMonitoring monitoring{100ms};
	// monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t)
		{
			std::this_thread::sleep_for(1000ms);
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	publisher->publish(0);

	highway->flush_tasks();
	highway->destroy();
} // monitor_hungs()

void monitor_debug()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});
	auto highway = hi::make_self_shared<hi::SerialHighWayDebug>(
		std::nullopt,
		nullptr,
		"SerialHighWayDebug:monitor_debug",
		std::move(logger),
		std::chrono::milliseconds{10});

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t i)
		{
			if (i == 0)
			{
				// BUG = 100 / i;
				std::this_thread::sleep_for(10ms);
			}
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	publisher->publish(0);

	highway->flush_tasks();
	highway->destroy();
} // monitor_debug

void monitor_and_repair_hungs()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});
	// auto highway = hi::make_self_shared<hi::SerialHighWay>(std::nullopt, nullptr,
	// "SerialHighWaySelfRepair:monitor_and_repair_hungs", std::move(logger), std::chrono::milliseconds{10});
	auto highway = hi::make_self_shared<hi::SerialHighWayDebug>(
		std::nullopt,
		nullptr,
		"SerialHighWaySelfRepair:monitor_and_repair_hungs",
		std::move(logger),
		std::chrono::milliseconds{10});

	hi::HighWaysMonitoring monitoring{100ms};
	monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t)
		{
			assert(highway->current_execution_on_this_highway());
			std::this_thread::sleep_for(100ms);
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	for (std::int32_t i = 0; i < 10; ++i)
	{
		publisher->publish(i);
		std::this_thread::sleep_for(100ms);
	}

	highway->flush_tasks();
} // monitor_and_repair_hungs()

void monitor_and_repair_hungs_concurrent_highway()
{
	auto logger = std::make_shared<hi::ErrorLogger>(
		[](std::string err)
		{
			std::cout << err << std::endl;
		});

	auto highway = hi::make_self_shared<hi::ConcurrentHighWayDebug>(
		std::nullopt,
		nullptr,
		"ConcurrentHighWayDebug:monitor_and_repair_hungs",
		std::move(logger),
		std::chrono::milliseconds{10},
		std::chrono::milliseconds{100});

	hi::HighWaysMonitoring monitoring{100ms};
	monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
		[&](std::int32_t)
		{
			std::this_thread::sleep_for(100ms);
		},
		highway->protector(),
		__FILE__,
		__LINE__);
	publisher->subscribe_channel()->subscribe(
		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway->mailbox()));

	// will increase_workers()
	for (std::int32_t i = 0; i < 1000; ++i)
	{
		publisher->publish(i);
		std::this_thread::sleep_for(10ms);
	}

	highway->flush_tasks();
} // monitor_and_repair_hungs_concurrent_highway

int main(int /* argc */, char ** /* argv */)
{
	//    monitor_shortage_of_holders();
	//	monitor_exception();
	//	monitor_hungs();
	//	monitor_debug();
	//	monitor_and_repair_hungs();
	monitor_and_repair_hungs_concurrent_highway();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
