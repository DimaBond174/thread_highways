#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

/*
	This example simulates a lack of holders.
	Holders help control the maximum amount of
	memory that can be spent on highway queues.
*/
void monitor_shortage_of_holders()
{
	hi::CoutScope scope("monitor_shortage_of_holders()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});

	auto highway =
		hi::make_self_shared<hi::SerialHighWay<>>("SerialHighWay:monitor_shortage_of_holders", std::move(logger));

	/*
	 * We set that the highway can operate with only one task.
	 * Nobody bothers to make the limit very large (as long as there is enough memory).
	 */
	highway->set_capacity(1);

	for (std::uint32_t i = 0; i < 10; ++i)
	{
		highway->post(
			[&, i]
			{
				scope.print(std::to_string(i));
			});
	}

	std::this_thread::sleep_for(100ms);
	highway->destroy();
} // monitor_shortage_of_holders()

// Example demonstrates catching exceptions
void monitor_exception()
{
	hi::CoutScope scope("monitor_exception()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>("SerialHighWay:monitor_exception", std::move(logger));

	highway->post(
		[]
		{
			throw std::logic_error("Task logic_error");
		});

	/*
		For a change, we will accept messages on PublishManyForOne
		- this class allows you to send messages from any thread to a single subscriber.
	*/
	hi::PublishManyForOne<std::int32_t> publisher{
		[&](std::int32_t)
		{
			throw std::logic_error("Subscription logic_error");
		},
		highway->protector_for_tests_only(),
		highway->mailbox(),
		false,
		__FILE__,
		__LINE__};

	// Send message
	publisher.publish(0);

	highway->flush_tasks();
	highway->destroy();
} // monitor_exception()

// Example demonstrates catching hungs
void monitor_hungs()
{
	hi::CoutScope scope("monitor_hungs()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});
	auto highway = hi::make_self_shared<hi::SerialHighWay<>>(
		"SerialHighWay:monitor_hungs",
		std::move(logger),
		std::chrono::milliseconds{10});

	highway->post(
		[&]
		{
			scope.print("Task hungs for 100ms");
			std::this_thread::sleep_for(100ms);
		},
		false,
		__FILE__,
		__LINE__);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::string>>();
	publisher->subscribe(
		[&](std::string publication)
		{
			scope.print(std::move(publication));
			std::this_thread::sleep_for(100ms);
		},
		highway->protector_for_tests_only(),
		highway->mailbox(),
		false,
		__FILE__,
		__LINE__);

	publisher->publish("Subscriber hungs for 100ms");

	highway->flush_tasks();
	highway->destroy();
} // monitor_hungs()

/*
If the application crashes, then there is not much you can do ..
Although not: you can send a log of what was before the crash
to the server where bugs from all users flock and find out exactly
what kind of code was working at that moment
*/
void monitor_SIGSEGV()
{
	hi::CoutScope scope("monitor_SIGSEGV()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});
	auto highway =
		hi::make_self_shared<hi::SerialHighWayDebug<>>("SerialHighWayDebug:monitor_SIGSEGV", std::move(logger));
	int zero = 0;
	highway->post(
		[&, zero]
		{
			scope.print("The next operation will destroy the application as with SIGSEGV, but the coordinates of the "
						"last run code will remain in the log");
			scope.print(std::to_string(100500 / zero));
		},
		false,
		__FILE__,
		__LINE__);

	highway->flush_tasks();
	highway->destroy();
} // monitor_SIGSEGV

// Example of hang detection and automatic self-repair
void monitor_and_repair_hungs()
{
	hi::CoutScope scope("monitor_and_repair_hungs()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});

	/*
	 * In the parameter, we indicate that the task
	 *  is considered to be hung if it runs longer than 10ms
	 */
	auto highway =
		hi::make_self_shared<hi::SerialHighWay<>>("SerialHighWay:monitor_and_repair_hungs", std::move(logger), 10ms);

	// We create a monitoring service, it will monitor every 100ms
	hi::HighWaysMonitoring monitoring{100ms};

	// We start monitoring the highway
	monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	publisher->subscribe(
		[&](std::int32_t i)
		{
			assert(highway->current_execution_on_this_highway());
			scope.print(std::to_string(i));
			// Hung
			std::this_thread::sleep_for(100ms);
		},
		highway->protector_for_tests_only(),
		highway->mailbox(),
		false,
		__FILE__,
		__LINE__);

	for (std::int32_t i = 0; i < 10; ++i)
	{
		// See log - thread id must change and exists text "going to restart because "
		publisher->publish(i);
		std::this_thread::sleep_for(100ms);
	}

	highway->flush_tasks();
	highway->destroy();
} // monitor_and_repair_hungs()

void monitor_and_repair_hungs_concurrent_highway()
{
	hi::CoutScope scope("monitor_and_repair_hungs_concurrent_highway()");
	auto logger = hi::create_default_logger(
		[&](std::string msg)
		{
			scope.print(std::move(msg));
		});

	auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>(
		"ConcurrentHighWay:monitor_and_repair_hungs",
		std::move(logger),
		10ms,
		100ms);

	hi::HighWaysMonitoring monitoring{100ms};
	monitoring.add_for_monitoring(highway);

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::int32_t>>();
	publisher->subscribe(
		[&](std::int32_t i)
		{
			assert(highway->current_execution_on_this_highway());
			scope.print(std::to_string(i));
			// Hung
			std::this_thread::sleep_for(100ms);
		},
		highway->protector_for_tests_only(),
		highway->mailbox(),
		false,
		__FILE__,
		__LINE__);

	// will increase_workers()
	for (std::int32_t i = 0; i < 100; ++i)
	{
		highway->post(
			[]
			{
				std::this_thread::sleep_for(100ms);
			},
			false,
			__FILE__,
			__LINE__);

		std::this_thread::sleep_for(100ms);
	}

	highway->flush_tasks();
	highway->destroy();
} // monitor_and_repair_hungs_concurrent_highway

int main(int /* argc */, char ** /* argv */)
{
	monitor_shortage_of_holders();
	monitor_exception();
	monitor_hungs();
	monitor_and_repair_hungs();
	monitor_and_repair_hungs_concurrent_highway();

	/*
	 * This monitor_SIGSEGV crash the program,
	 * but a trace with the coordinates of the problematic code will remain in the log
	 */
	monitor_SIGSEGV();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
