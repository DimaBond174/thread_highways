#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

std::shared_ptr<hi::HighWay> render_highway()
{
        static hi::RAIIdestroy highway{ hi::make_self_shared<hi::HighWay>() };
	return highway.object_;
}

void create_subscriber(
        const hi::ISubscribeHerePtr<std::string> & channel,
        std::shared_ptr<hi::CoutScope> scope,
        std::int32_t life_time)
{
    struct SubscriptionSelfProtector
    {
        std::shared_ptr<hi::ISubscription<std::string>> subscription_;
    };
    auto self_protector = std::make_shared<SubscriptionSelfProtector>();
    auto subscription = channel->subscribe(
                [scope = std::move(scope), protector = self_protector, life_time, name = life_time](std::string publication) mutable
                {
                        scope->print(std::to_string(name).append(" life_time subscriber received: ")
                                                         .append(std::move(publication))
                                                         .append(", life_time=")
                                                         .append(std::to_string(life_time)));

                        if (--life_time > 0)
                        {
                                return;
                        }
                        // Break subscription
                        protector.reset();

                        scope->print(std::to_string(name).append(" life_time subscriber gone"));
                        // The lambda with the captured scope remains in the holders of the highway
                        // until the holder is reused. To release the scope I do this
                        scope.reset();
                }, false);
    self_protector->subscription_ = std::move(subscription);
}

void connections_notifier_simple()
{
	auto scope = std::make_shared<hi::CoutScope>("connections_notifier_simple()");
    auto highway = render_highway();
    const auto publisher = hi::make_self_shared<hi::HighWayStickyPublisherWithConnectionsNotifier<std::string>>(
		[&]
		{
			scope->print("First subscriber connected");
		},
		[&]
		{
			scope->print("Last subscriber disconnected");
        }, highway);

        {
            auto subscriber1 = publisher->subscribe_channel()->subscribe([&](std::string publication)
            {
                scope->print(std::string{"subscriber1: "}.append(std::move(publication)));
            }, false);

            publisher->publish("publication1");
            highway->flush_tasks();
        }

        publisher->publish("publication2");
        highway->flush_tasks();
} // connections_notifier_simple()

void connections_notifier_start_stop_microservice()
{
    auto scope = std::make_shared<hi::CoutScope>("connections_notifier_start_stop_microservice()");

    class Service
    {
    public:
                Service(std::shared_ptr<hi::HighWay> highway, std::shared_ptr<hi::CoutScope> scope)
                    : highway_{hi::make_weak_ptr(std::move(highway))}
                        , scope_{std::move(scope)}
                        , publisher_{hi::make_self_shared<hi::HighWayStickyPublisherWithConnectionsNotifier<std::string>>(
                  [&]
                  {
                      on_first_connected();
                  },
                  [&]
                  {
                      on_last_disconnected();
                  }, highway_)}
        {
        }

        ~Service()
        {
            scope_->print("Service: destroyed.");
        }
        Service(const Service &) = delete;
        Service & operator=(const Service &) = delete;
        Service(Service &&) = delete;
        Service & operator=(Service &&) = delete;

        hi::ISubscribeHerePtr<std::string> subscribe_channel()
        {
            return publisher_->subscribe_channel();
        }

    private:
        void on_first_connected()
        {
            scope_->print("Service: first subscriber connected");
            protector_ = std::make_shared<bool>();
                        highway_->schedule(
                                [&](hi::Schedule& schedule)
                {
                    service_loop(schedule);
                },
                std::weak_ptr(protector_),
                                {},
                __FILE__,
                __LINE__);
        }

        void on_last_disconnected()
        {
            scope_->print("Service: last subscriber disconnected");

            // Remove service from highway
            protector_.reset();
        }

                void service_loop(hi::Schedule & schedule)
        {
            ++data_;
            scope_->print(std::string{"Service: service_loop: "}.append(std::to_string(data_)));
            publisher_->publish(std::to_string(data_));

            // Schedule next start
            schedule.schedule_launch_in(50ms);
        }

    private:
                const hi::HighWayWeakPtr highway_;
                const std::shared_ptr<hi::CoutScope> scope_;
                const std::shared_ptr<hi::HighWayStickyPublisherWithConnectionsNotifier<std::string>> publisher_;
        std::shared_ptr<bool> protector_;
        std::uint32_t data_{0};
    };

    {
                Service service{render_highway(), scope};
        auto channel = service.subscribe_channel();
        create_subscriber(channel, scope, 5);
        scope->print("wait while subscriber alive\n\n");
        std::this_thread::sleep_for(1s);
        scope->print("+++++++++++++++++++\n\n ");
        std::this_thread::sleep_for(1s);
        scope->print("2 subscribers with long and short life\n\n");
        create_subscriber(channel, scope, 5);
        create_subscriber(channel, scope, 100500);
        std::this_thread::sleep_for(1s);
    }

} // connections_notifier_start_stop_microservice()

int main(int /* argc */, char ** /* argv */)
{
	connections_notifier_simple();
    connections_notifier_start_stop_microservice();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
