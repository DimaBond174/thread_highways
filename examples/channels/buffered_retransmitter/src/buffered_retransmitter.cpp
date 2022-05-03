#include <thread_highways/include_all.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

using namespace std::chrono_literals;

void common_test(
	std::shared_ptr<hi::PublishOneForMany<std::uint32_t>> & publisher,
	hi::ISubscribeHerePtr<std::uint32_t> subscribe_channel)
{
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	auto highway_mailbox = highway->mailbox();

	std::promise<std::uint32_t> promise;
	auto future = promise.get_future();
	std::future_status status;
	auto wait_for_result = [&]
	{
		switch (status = future.wait_for(1s); status)
		{
		case std::future_status::deferred:
			std::cout << "deferred\n";
			break;
		case std::future_status::timeout:
			std::cout << "nothing\n";
			break;
		case std::future_status::ready:
			std::cout << "future= " << future.get() << std::endl;
			break;
		}
		promise = {};
		future = promise.get_future();
	};

	auto subscription_callback = hi::SubscriptionCallback<std::uint32_t>::create(
		[&](std::uint32_t publication, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			promise.set_value(publication);
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);

	std::cout << "On subscribe: ";
	subscribe_channel->subscribe(
		hi::Subscription<std::uint32_t>::create(std::move(subscription_callback), highway_mailbox));
	wait_for_result();

	std::cout << "On send same: ";
	publisher->publish(12345);
	wait_for_result();

	std::cout << "On send new value: ";
	publisher->publish(67890);
	wait_for_result();

	highway->destroy();
}

void test_atomic_resend_new_only()
{
	std::cout << "=====================================\n"
			  << " test_atomic_resend_new_only() \n"
			  << "=====================================\n";

	// std::uint32_t can be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto buffered_retransmitter =
		hi::make_self_shared<hi::BufferedRetransmitter<std::uint32_t>>(publisher->subscribe_channel(), 12345);

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_atomic_resend_new_only

void test_atomic_resend_only()
{
	std::cout << "=====================================\n"
			  << " test_atomic_resend_only() \n"
			  << "=====================================\n";

	// std::uint32_t can be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::uint32_t, true, false>>(
		publisher->subscribe_channel(),
		12345);

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_atomic_resend_only

void test_atomic_new_only()
{
	std::cout << "=====================================\n"
			  << " test_atomic_new_only() \n"
			  << "=====================================\n";

	// std::uint32_t can be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::uint32_t, false, true>>(
		publisher->subscribe_channel(),
		12345);

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_atomic_new_only

void test_atomic()
{
	std::cout << "=====================================\n"
			  << " test_atomic() \n"
			  << "=====================================\n";

	// std::uint32_t can be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::uint32_t, false, false>>(
		publisher->subscribe_channel(),
		12345);

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_atomic

void common_test(
	std::shared_ptr<hi::PublishOneForMany<std::string>> & publisher,
	hi::ISubscribeHerePtr<std::string> subscribe_channel)
{
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	auto highway_mailbox = highway->mailbox();

	std::promise<std::string> promise;
	auto future = promise.get_future();
	std::future_status status;
	auto wait_for_result = [&]
	{
		switch (status = future.wait_for(1s); status)
		{
		case std::future_status::deferred:
			std::cout << "deferred\n";
			break;
		case std::future_status::timeout:
			std::cout << "nothing\n";
			break;
		case std::future_status::ready:
			std::cout << "future= " << future.get() << std::endl;
			break;
		}
		promise = {};
		future = promise.get_future();
	};

	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
		[&](std::string publication, const std::atomic<std::uint32_t> &, const std::uint32_t)
		{
			promise.set_value(publication);
		},
		highway->protector_for_tests_only(),
		__FILE__,
		__LINE__);

	std::cout << "On subscribe: ";
	subscribe_channel->subscribe(
		hi::Subscription<std::string>::create(std::move(subscription_callback), highway_mailbox));
	wait_for_result();

	std::cout << "On send same: ";
	publisher->publish("Mother washed the frame");
	wait_for_result();

	std::cout << "On send new value: ";
	publisher->publish("new value");
	wait_for_result();

	highway->destroy();
}

void test_with_mutex_resend_new_only()
{
	std::cout << "=====================================\n"
			  << " test_with_mutex_resend_new_only() \n"
			  << "=====================================\n";

	// std::string can't be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::string>>(
		publisher->subscribe_channel(),
		"Mother washed the frame");

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_with_mutex

void test_with_mutex_resend_only()
{
	std::cout << "=====================================\n"
			  << " test_with_mutex_resend_only() \n"
			  << "=====================================\n";

	// std::string can't be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::string, true, false>>(
		publisher->subscribe_channel(),
		"Mother washed the frame");

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_with_mutex_resend_only

void test_with_mutex_new_only()
{
	std::cout << "=====================================\n"
			  << " test_with_mutex_new_only() \n"
			  << "=====================================\n";

	// std::string can't be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::string, false, true>>(
		publisher->subscribe_channel(),
		"Mother washed the frame");

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_with_mutex_new_only

void test_with_mutex()
{
	std::cout << "=====================================\n"
			  << " test_with_mutex() \n"
			  << "=====================================\n";

	// std::string can't be stored in atomic
	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	auto buffered_retransmitter = hi::make_self_shared<hi::BufferedRetransmitter<std::string, false, false>>(
		publisher->subscribe_channel(),
		"Mother washed the frame");

	common_test(publisher, buffered_retransmitter->subscribe_channel());

	std::cout << "Current buffered vaule: " << buffered_retransmitter->get_value() << std::endl;
} // test_with_mutex

int main(int /* argc */, char ** /* argv */)
{
	test_atomic_resend_new_only();
	test_atomic_resend_only();
	test_atomic_new_only();
	test_atomic();

	test_with_mutex_resend_new_only();
	test_with_mutex_resend_only();
	test_with_mutex_new_only();
	test_with_mutex();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
