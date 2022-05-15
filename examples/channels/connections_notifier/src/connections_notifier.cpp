#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

std::shared_ptr<hi::SerialHighWayWithScheduler<>> render_highway()
{
	static hi::RAIIdestroy highway{
		hi::make_self_shared<hi::SerialHighWayWithScheduler<>>("RenderThread", nullptr, 10ms, 10ms)};
	return highway.object_;
}

void create_subscriber(
	hi::ISubscribeHerePtr<std::string> channel,
	std::shared_ptr<hi::CoutScope> scope,
	std::int32_t life_time)
{
	auto simple_protector = std::make_shared<bool>();
	auto weak_from_protector = std::weak_ptr(simple_protector);
	hi::subscribe(
		channel,
		[scope = std::move(scope), protector = std::move(simple_protector), life_time](std::string publication) mutable
		{
			scope->print(std::string{"life_time subscriber received: "}
							 .append(std::move(publication))
							 .append(", life_time=")
							 .append(std::to_string(life_time)));

			if (--life_time > 0)
			{
				return;
			}
			// Break subscription
			protector.reset();

			scope->print("subscriber gone");
			// The lambda with the captured scope remains in the holders of the highway
			// until the holder is reused. To release the scope I do this
			scope.reset();
		},
		std::move(weak_from_protector),
		render_highway()->mailbox());
}

void connections_notifier_simple()
{
	auto scope = std::make_shared<hi::CoutScope>("connections_notifier_simple()");
	auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();

	const auto publisher = hi::make_self_shared<hi::PublishOneForManyWithConnectionsNotifier<std::string>>(
		[&]
		{
			scope->print("First subscriber connected");
		},
		[&]
		{
			scope->print("Last subscriber disconnected");
		});

	auto simple_protector = std::make_shared<bool>();
	auto weak_from_protector = std::weak_ptr(simple_protector);
	publisher->subscribe(
		[&, protector = std::move(simple_protector)](std::string publication) mutable
		{
			scope->print(std::string{"single shot subscriber1 received: "}.append(std::move(publication)));
			// Break subscription
			protector.reset();
		},
		std::move(weak_from_protector),
		highway->mailbox());

	// Create subscriber2 without publisher object
	create_subscriber(publisher->subscribe_channel(), scope, 1);

	publisher->publish("publication1");

	// Wait until publication will be received by subscriber on highway
	// and the subscriber will have time to break the protector
	highway->flush_tasks();

	// This publication will not be delivered because the protector is already broken
	publisher->publish("publication2");

	highway->flush_tasks();
	highway->destroy();
} // connections_notifier_simple()

void connections_notifier_start_stop_microservice()
{
	auto scope = std::make_shared<hi::CoutScope>("connections_notifier_start_stop_microservice()");

	class Service
	{
	public:
		Service(std::shared_ptr<hi::CoutScope> scope)
			: scope_{std::move(scope)}
			, publisher_{hi::make_self_shared<hi::PublishOneForManyWithConnectionsNotifier<std::string>>(
				  [&]
				  {
					  on_first_connected();
				  },
				  [&]
				  {
					  on_last_disconnected();
				  })}
		{
		}

		~Service()
		{
			scope_->print("Service: destroyed.");
			scope_.reset();
		}

		hi::ISubscribeHerePtr<std::string> subscribe_channel()
		{
			return publisher_->subscribe_channel();
		}

	private:
		void on_first_connected()
		{
			scope_->print("Service: first subscriber connected");
			protector_ = std::make_shared<bool>();
			render_highway()->add_reschedulable_runnable(
				[&](hi::ReschedulableRunnable::Schedule & schedule)
				{
					service_loop(schedule);
				},
				std::weak_ptr(protector_),
				__FILE__,
				__LINE__);
		}

		void on_last_disconnected()
		{
			scope_->print("Service: last subscriber disconnected");

			// Remove service from highway
			protector_.reset();
		}

		void service_loop(hi::ReschedulableRunnable::Schedule & schedule)
		{
			++data_;
			scope_->print(std::string{"Service: service_loop: "}.append(std::to_string(data_)));
			publisher_->publish(std::to_string(data_));

			// Schedule next start
			schedule.schedule_launch_in(50ms);
		}

	private:
		std::shared_ptr<hi::CoutScope> scope_;
		const std::shared_ptr<hi::PublishOneForManyWithConnectionsNotifier<std::string>> publisher_;
		std::shared_ptr<bool> protector_;
		std::uint32_t data_{0};
	};

	{
		Service service{scope};
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
