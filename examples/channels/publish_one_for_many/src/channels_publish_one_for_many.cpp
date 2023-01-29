#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("channels_publish_many_for_one");

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<int>>();
	auto channel = publisher->subscribe_channel();

    const auto subscribe = [&](hi::HighWayWeakPtr highway)
	{
        return channel->subscribe(
			[&](int publication)
			{
				scope.print(std::to_string(publication));
			},
            highway, __FILE__, __LINE__);
	};

    std::vector<std::shared_ptr<hi::HighWay>> highways;
    std::vector<std::shared_ptr<hi::ISubscription<int>>> subscriptions;
	for (int i = 0; i < 10; ++i)
	{
        auto it = highways.emplace_back(hi::make_self_shared<hi::HighWay>());
        subscriptions.emplace_back(subscribe(hi::make_weak_ptr(it)));
	}

	for (int i = 0; i < 100; ++i)
	{
		publisher->publish(i);
	}

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

	for (auto && it : highways)
	{
		it->destroy();
	}

	std::cout << "Test finished" << std::endl;
	return 0;
}
