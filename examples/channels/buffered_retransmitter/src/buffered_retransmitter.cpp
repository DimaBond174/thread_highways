#include <thread_highways/include_all.h>

#include <future>
#include <string>

using namespace std::chrono_literals;

namespace
{

inline std::shared_ptr<hi::HighWay> highway()
{
    static auto highway = hi::make_self_shared<hi::HighWay>();
    return highway;
}

}

template<typename T>
void common_test(
    T& publisher,  const hi::ISubscribeHerePtr<std::uint32_t> & subscribe_channel, bool for_new_only)
{
    publisher->publish(12345);

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

    auto  subscription = subscribe_channel->subscribe(
    [&](std::uint32_t publication)
    {
        promise.set_value(publication);
    },
    highway(),
            __FILE__,
            __LINE__,
            false,
            for_new_only);

	std::cout << "On subscribe: ";
	wait_for_result();

    std::cout << "On send: ";
    publisher->publish(12345);
    wait_for_result();

    std::cout << "On send same: ";
    publisher->publish(12345);
    wait_for_result();

	std::cout << "On send new value: ";
	publisher->publish(67890);
	wait_for_result();
}

void test_send()
{
	std::cout << "=====================================\n"
              << " test_send() \n"
			  << "=====================================\n";

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
    common_test(publisher, publisher->subscribe_channel(), false);
} // test_send

void test_send_new_only()
{
    std::cout << "=====================================\n"
              << " test_send_new_only() \n"
              << "=====================================\n";

    auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
    common_test(publisher, publisher->subscribe_channel(), true);
} // test_send_new_only

void test_HighWayStickyPublisher()
{
    std::cout << "=====================================\n"
              << " test_HighWayStickyPublisher() \n"
              << "=====================================\n";
    auto publisher = hi::make_self_shared<hi::HighWayStickyPublisher<std::uint32_t>>(highway());
    common_test(publisher, publisher->subscribe_channel(), true);
} // test_send_new_only

int main(int /* argc */, char ** /* argv */)
{
    test_send();
    test_send_new_only();
    test_HighWayStickyPublisher();

    highway()->destroy();
	std::cout << "Tests finished" << std::endl;
	return 0;
}
