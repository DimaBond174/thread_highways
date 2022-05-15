#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

using namespace std::chrono_literals;

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("channels_publish_many_for_one");
	// This is a very simple example of sending messages to one subscriber.
	// And so that you are not bored at all, let's see alternative methods.

	// RAIIdestroy will automatically destroy the highway when exiting the scope
	hi::RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>(
		"HighWay",
		nullptr,
		10ms, // max_task_execution_time
		1ms // workers_change_period
		)};

	auto highway_mailbox = highway.object_->mailbox();

	// So it is also possible
	highway_mailbox->send_may_blocked(hi::Runnable::create(
		[]()
		{
			// Just a handy thing for debugging - you can give names to threads
			hi::set_this_thread_name("highway");
		},
		__FILE__,
		__LINE__));

	auto subscription_callback = hi::SubscriptionCallback<std::string>::create(
		[&](std::string publication, const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id)
		{
			if (your_run_id != global_run_id)
			{
				// highway thread has stopped
				return;
			}
			scope.print(std::move(publication));
		},
		highway.object_->protector_for_tests_only(),
		__FILE__,
		__LINE__);

	hi::PublishManyForOne<std::string> publisher{
		hi::Subscription<std::string>::create(std::move(subscription_callback), highway_mailbox)};

	const auto fun = [&](std::uint32_t id)
	{
		std::string str{"worker"};
		str.append(std::to_string(id));

		hi::set_this_thread_name(str);
		str.append(" data : ");

		for (int i = 0; i <= 50; ++i)
		{
			publisher.publish(std::string{str}.append(std::to_string(i)));
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

	publisher.publish(std::string{"Mother washed the frame"});

	for (auto && it : threads)
	{
		it.join();
	}

	highway.object_->flush_tasks();

	std::cout << "Test finished" << std::endl;
	return 0;
}
