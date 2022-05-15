#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("channels_publish_many_for_one");

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<int>>();
	auto channel = publisher->subscribe_channel();

	const auto subscribe = [&](hi::IHighWayPtr highway)
	{
		hi::subscribe(
			channel,
			[&](int publication)
			{
				scope.print(std::to_string(publication));
			},
			highway->protector_for_tests_only(),
			highway->mailbox());
	};

	std::vector<hi::IHighWayPtr> highways;
	for (int i = 0; i < 10; ++i)
	{
		auto it = highways.emplace_back(hi::make_self_shared<hi::ConcurrentHighWay<>>());
		subscribe(it);
	}

	for (int i = 0; i < 100; ++i)
	{
		publisher->publish(i);
	}

	for (auto && it : highways)
	{
		it->flush_tasks();
		it->destroy();
	}

	std::cout << "Test finished" << std::endl;
	return 0;
}
