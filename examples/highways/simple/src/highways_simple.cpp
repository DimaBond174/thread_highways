#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <future>

using namespace std::chrono_literals;

void post_lambda_on_highway()
{
	hi::CoutScope scope("post_lambda_on_highway()");

	// Create executor
    const auto highway = hi::make_self_shared<hi::HighWay>();

	// Some data that task need to work
	auto unique = std::make_unique<std::string>("hello to std::function<> ;))");

	// Post task
    highway->execute(
        [&, unique = std::move(unique)]
        {
            scope.print(std::string{"unique: "}.append(*unique));
        });

	// Let's wait until the highway completes all the posted tasks
	highway->flush_tasks();

	// Stop threads
	highway->destroy();
} // post_lambda_on_highway

// Example demonstrates canceling tasks
void using_protector()
{
	const auto scope = std::make_shared<hi::CoutScope>("using_protector()");

	// Create executor
    const auto highway = hi::make_self_shared<hi::HighWay>();

	struct SelfProtectedTask
	{
		SelfProtectedTask(
			std::weak_ptr<SelfProtectedTask> self_weak,
			std::string message,
            hi::HighWay& highway,
			std::shared_ptr<hi::CoutScope> scope)
			: self_weak_{std::move(self_weak)}
			, message_{std::move(message)}
		{
			/*
				Usually subscriptions and regular tasks are created directly in the service constructor
				But these tasks should not live longer than the service object itself.
				Here we use {self_weak_} as a protector.
			*/
            highway.execute(
                [this, scope]
				{
                    scope->print(message_);
				},
                self_weak_, __FILE__, __LINE__);
		}

		const std::weak_ptr<SelfProtectedTask> self_weak_;
		const std::string message_;
	};

	{ // scope
        auto service1 = hi::make_self_shared<SelfProtectedTask>("service1 shouldn't start", *highway, scope);
	}
    auto service2 = hi::make_self_shared<SelfProtectedTask>("service2 must start", *highway, scope);

	// Let's wait until the highway completes all the posted tasks
	highway->flush_tasks();

	// Stop threads
	highway->destroy();
} // using_protector

void multithreading()
{
	hi::CoutScope scope("multithreading()");

    hi::RAIIdestroy highway{hi::make_self_shared<hi::MultiThreadedTaskProcessingPlant>(10 /* threads */)};
    highway.object_->set_capacity(100);

    std::atomic<std::int32_t> j{0};
	for (std::int32_t i = 0; i < 100; ++i)
	{
        highway.object_->execute(
			[&, i]
			{
				scope.print(std::to_string(i));
                ++j;
				std::this_thread::sleep_for(10ms);
			});
	}

    while(j < 100)
    {
        std::this_thread::sleep_for(10ms);
    }

    for (std::int32_t i = 100; i < 300; ++i)
    {
        if (!highway.object_->try_execute(
            [&, i]
            {
                scope.print(std::to_string(i));
            }))
        {
            scope.print("no more task holders on MultiThreadedTaskProcessingPlant");
        }
    }

    std::this_thread::sleep_for(1000ms);
} // multithreading

// Example demonstrates stopping a long-running task
void long_running_task()
{
	hi::CoutScope scope("long_running_task()");

	std::promise<bool> start_promise;
	auto start_future = start_promise.get_future();

	std::promise<bool> stop_promise;
	auto stop_future = stop_promise.get_future();

	{
        hi::RAIIdestroy highway{
            hi::make_self_shared<hi::HighWay>(
            [&](const hi::Exception& ex)
            {
                scope.print(ex.what_as_string() + " in " + ex.file_name() + ": " + std::to_string(ex.file_line()));
            } /* Exceptions handler */,
            "HighWay with time control" /* highway name*/,
            std::chrono::milliseconds{1} /* max task execution time */)
        };

        highway.object_->execute(
            [&](const std::atomic<bool>& keep_execution)
			{
				// Say task started
				start_promise.set_value(true);
				// Long running algorithm
                while (keep_execution.load(std::memory_order_acquire))
				{
					std::this_thread::sleep_for(10ms);
				}
				// Say task stopped
				stop_promise.set_value(true);
            }, __FILE__, __LINE__);
		scope.print(std::string{"Task started:"}.append(std::to_string(start_future.get())));
	}

	scope.print(std::string{"Task stopped:"}.append(std::to_string(stop_future.get())));
} // long_running_task

int main(int /* argc */, char ** /* argv */)
{
    post_lambda_on_highway();
    using_protector();
    multithreading();
    long_running_task();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
