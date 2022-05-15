#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("channels_publish_many_for_many");

	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	auto highway_mailbox = highway->mailbox();
	auto fake_protector = highway->protector_for_tests_only();

	auto publisher = hi::make_self_shared<hi::PublishManyForMany<std::string>>();
	auto channel = publisher->subscribe_channel();

	const auto fun = [&](std::uint32_t id)
	{
		hi::subscribe(
			channel,
			[&, id](std::string publication)
			{
				scope.print(std::to_string(id).append(") subscriber received: ").append(publication));
			},
			fake_protector,
			highway_mailbox);

		for (int i = 0; i <= 10; ++i)
		{
			publisher->publish(std::to_string(id).append("thread:").append(std::to_string(i)));
		}
	};

	std::vector<std::thread> threads;
	for (std::uint32_t id = 0; id < 5; ++id)
	{
		threads.emplace_back(
			[&, id]
			{
				fun(id);
			});
	}

	publisher->publish(std::string{"Mother washed the frame"});

	for (auto && it : threads)
	{
		it.join();
	}

	highway->flush_tasks();
	highway->destroy();

	std::cout << "Test finished" << std::endl;
	return 0;
}
