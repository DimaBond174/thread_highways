#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <future>

using namespace std::chrono_literals;

void post_lambda_on_highway()
{
	hi::CoutScope scope("post_lambda_on_highway()");

	// Create executor
	const auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	// Some data that task need to work
	auto unique = std::make_unique<std::string>("hello to std::function<> ;))");

	// Post task
	highway->post(
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
	const auto highway = hi::make_self_shared<hi::SerialHighWay<>>();

	struct SelfProtectedTask
	{
		SelfProtectedTask(
			std::weak_ptr<SelfProtectedTask> self_weak,
			std::string message,
			hi::IHighWayPtr highway,
			std::shared_ptr<hi::CoutScope> scope)
			: self_weak_{std::move(self_weak)}
			, message_{std::move(message)}
			, highway_{std::move(highway)}
			, scope_{std::move(scope)}
		{
			/*
				Usually subscriptions and regular tasks are created directly in the service constructor
				But these tasks should not live longer than the service object itself.
				Here we use {self_weak_} as a protector.
			*/
			highway_->post(
				[&]
				{
					scope_->print(message_);
				},
				self_weak_);
		}

		const std::weak_ptr<SelfProtectedTask> self_weak_;
		const std::string message_;
		const hi::IHighWayPtr highway_;
		const std::shared_ptr<hi::CoutScope> scope_;
	};

	{ // scope
		auto service1 = hi::make_self_shared<SelfProtectedTask>("service1 shouldn't start", highway, scope);
	}
	auto service2 = hi::make_self_shared<SelfProtectedTask>("service2 must start", highway, scope);

	// Let's wait until the highway completes all the posted tasks
	highway->flush_tasks();

	// Stop threads
	highway->destroy();
} // using_protector

// Example demonstrates highway capacity building
void multithreading()
{
	hi::CoutScope scope("multithreading()");

	hi::RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>(
		"HighWay",
		hi::create_default_logger(
			[&](std::string msg)
			{
				scope.print(std::move(msg));
			}),
		10ms, // max_task_execution_time
		1ms // workers_change_period
		)};

	for (std::int32_t i = 0; i < 100; ++i)
	{
		highway.object_->post(
			[&, i]
			{
				scope.print(std::to_string(i));
				std::this_thread::sleep_for(10ms);
			});
	}

	highway.object_->flush_tasks();
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
		hi::RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};

		highway.object_->post(
			[&](const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id)
			{
				// Say task started
				start_promise.set_value(true);
				// Long running algorithm
				while (global_run_id == your_run_id)
				{
					std::this_thread::sleep_for(10ms);
				}
				// Say task stopped
				stop_promise.set_value(true);
			});
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
