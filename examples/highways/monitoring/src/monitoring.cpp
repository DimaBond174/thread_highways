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
        auto highway =  hi::make_self_shared<hi::HighWay>();

	/*
	 * We set that the highway can operate with only one task.
	 * Nobody bothers to make the limit very large (as long as there is enough memory).
	 */
	highway->set_capacity(1);

	for (std::uint32_t i = 0; i < 10; ++i)
	{
                if (!highway->try_execute(
			[&, i]
			{
				scope.print(std::to_string(i));
                        }))
                {
                    scope.print("no more task holders on HighWay");
                }
	}

	std::this_thread::sleep_for(100ms);
	highway->destroy();
} // monitor_shortage_of_holders()

// Example demonstrates catching exceptions
void monitor_exception()
{
	hi::CoutScope scope("monitor_exception()");
        auto highway =
                hi::make_self_shared<hi::HighWay>(
                    [&](const hi::Exception& ex)
                    {
                        scope.print(ex.what_as_string() + " in " + ex.file_name() + ": " + std::to_string(ex.file_line()));
                    } /* Exceptions handler */
                    );

        highway->execute(
		[]
		{
			throw std::logic_error("Task logic_error");
		});

	highway->flush_tasks();
	highway->destroy();
} // monitor_exception()

// Example demonstrates catching hungs
void monitor_hungs()
{
	hi::CoutScope scope("monitor_hungs()");
        hi::RAIIdestroy highway{
            hi::make_self_shared<hi::HighWay>(
            [&](const hi::Exception& ex)
            {
                scope.print(ex.what_as_string() + " in " + ex.file_name() + ": " + std::to_string(ex.file_line()));
            } /* Exceptions handler */,
            "HighWay with time control" /* highway name*/,
            std::chrono::milliseconds{1} /* max task execution time */)
        };

    auto publisher = hi::make_self_shared<hi::HighWayPublisher<std::string>>(highway.object_);
    auto channel = publisher->subscribe_channel();
    auto subscription = publisher->subscribe_channel()->subscribe(
        [&](std::string publication)
        {
            scope.print(std::move(publication));
            std::this_thread::sleep_for(10ms);
        }, highway.object_, __FILE__, __LINE__, false, true);

    publisher->publish("Subscriber hungs for 10ms");

    highway->execute(
    [&]
    {
        scope.print("Task hungs for 10ms");
        std::this_thread::sleep_for(10ms);
    },
    __FILE__,
    __LINE__);

	highway->flush_tasks();
	highway->destroy();
} // monitor_hungs()

int main(int /* argc */, char ** /* argv */)
{
	monitor_shortage_of_holders();
	monitor_exception();
	monitor_hungs();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
