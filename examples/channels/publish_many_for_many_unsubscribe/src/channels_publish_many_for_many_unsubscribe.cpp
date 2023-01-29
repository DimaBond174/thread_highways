#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

using namespace std::chrono_literals;

int main(int /* argc */, char ** /* argv */)
{
	auto scope = std::make_shared<hi::CoutScope>("channels_publish_many_for_many_unsubscribe");
        auto highway = hi::make_self_shared<hi::HighWay>();
        auto publisher = hi::make_self_shared<hi::HighWayPublisher<std::string>>(highway);
	auto channel = publisher->subscribe_channel();

	struct SelfProtectedSubscriber
	{
		SelfProtectedSubscriber(
			const std::uint32_t id,
			std::shared_ptr<hi::CoutScope> scope,
                        const hi::ISubscribeHerePtr<std::string> & channel)
			: id_{id}
			, scope_{std::move(scope)}
                        , subscription_{
                              channel->subscribe([id = id_, scope = scope_](std::string publication)
                {
                        scope->print(std::to_string(id).append(") subscriber received: ").append(publication));
                }, false)
                }
		{
		}

		~SelfProtectedSubscriber()
		{
			scope_->print(std::to_string(id_).append(") subscriber destroyed.\n\n"));
		}
		SelfProtectedSubscriber(const SelfProtectedSubscriber &) = delete;
		SelfProtectedSubscriber & operator=(const SelfProtectedSubscriber &) = delete;
		SelfProtectedSubscriber(SelfProtectedSubscriber &&) = delete;
		SelfProtectedSubscriber & operator=(SelfProtectedSubscriber &&) = delete;

		const std::uint32_t id_;
		const std::shared_ptr<hi::CoutScope> scope_;
                const std::shared_ptr<hi::ISubscription<std::string>> subscription_;
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
        SelfProtectedSubscriber subscriber1{1, scope, channel};

	{
		// Short Life Subscriber
                SelfProtectedSubscriber subscriber2{2, scope, channel};
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
