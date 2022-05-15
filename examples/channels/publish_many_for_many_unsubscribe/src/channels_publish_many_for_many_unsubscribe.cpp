#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

using namespace std::chrono_literals;

int main(int /* argc */, char ** /* argv */)
{
	auto scope = std::make_shared<hi::CoutScope>("channels_publish_many_for_many_unsubscribe");
	auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();
	auto highway_mailbox = highway->mailbox();

	auto publisher = hi::make_self_shared<hi::PublishManyForManyCanUnSubscribe<std::string>>();
	auto channel = publisher->subscribe_channel();

	struct SelfProtectedSubscriber
	{
		SelfProtectedSubscriber(
			std::weak_ptr<SelfProtectedSubscriber> self_weak,
			const std::uint32_t id,
			std::shared_ptr<hi::CoutScope> scope,
			const hi::ISubscribeHerePtr<std::string> & channel,
			hi::IHighWayMailBoxPtr highway_mailbox)
			: id_{id}
			, scope_{std::move(scope)}
		{
			hi::subscribe(
				channel,
				[id = id_, scope = scope_](std::string publication)
				{
					scope->print(std::to_string(id).append(") subscriber received: ").append(publication));
				},
				std::move(self_weak),
				std::move(highway_mailbox));
		}
		~SelfProtectedSubscriber()
		{
			scope_->print(std::to_string(id_).append(") subscriber destroyed.\n\n"));
		}

		const std::uint32_t id_;
		const std::shared_ptr<hi::CoutScope> scope_;
	};

	const auto fun = [&](std::uint32_t id)
	{
		for (int i = 0; i <= 50; ++i)
		{
			publisher->publish(std::to_string(id).append("thread:").append(std::to_string(i)));
			std::this_thread::sleep_for(10ms);
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

	// Long Life Subscriber
	auto subscriber1 = hi::make_self_shared<SelfProtectedSubscriber>(1, scope, channel, highway_mailbox);

	{
		// Short Life Subscriber
		auto subscriber2 = hi::make_self_shared<SelfProtectedSubscriber>(2, scope, channel, highway_mailbox);
		std::this_thread::sleep_for(100ms);
	}

	for (auto && it : threads)
	{
		it.join();
	}

	highway->flush_tasks();
	highway->destroy();

	std::cout << "Test finished" << std::endl;
	return 0;
}
