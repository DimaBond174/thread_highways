/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

void schedule_simple_runnable()
{
	hi::CoutScope scope("schedule_simple_runnable()");
	auto logger = hi::create_default_logger(
		[&](const std::string & msg)
		{
			scope.print(msg);
		});
	auto highway = hi::make_self_shared<hi::SingleThreadHighWayWithScheduler<>>(
		"SingleThreadHighWay:schedule_simple_runnable",
		std::move(logger),
		10ms,
		10ms);

	std::promise<bool> promise;
	auto future = promise.get_future();

	highway->add_reschedulable_runnable(
		[&, i = 0](hi::ReschedulableRunnable::Schedule & schedule) mutable
		{
			++i;
			scope.print(std::string{"number of launches: "}.append(std::to_string(i)));
			if (i < 10)
			{
				schedule.schedule_launch_in(10ms);
			}
			else
			{
				promise.set_value(true);
			}
		},
		__FILE__,
		__LINE__);

	future.wait();

	highway->destroy();
} // schedule_simple_runnable()

// Example of a regularly executed service
void schedule_functor()
{
	auto highway =
		hi::make_self_shared<hi::SerialHighWayWithScheduler<>>("SerialHighWay:serial_schedule", nullptr, 10ms, 10ms);

	struct Service
	{
		void operator()(hi::ReschedulableRunnable::Schedule & schedule)
		{
			++starts_;
			scope->print(std::to_string(starts_));
			// Schedule next start
			schedule.schedule_launch_in(50ms);
		}

		const std::shared_ptr<hi::CoutScope> scope{std::make_shared<hi::CoutScope>("Service")};
		std::uint32_t starts_{0};
	};

	highway->add_reschedulable_runnable(Service{}, __FILE__, __LINE__);
	std::this_thread::sleep_for(1s);

	highway->destroy();
} // schedule_functor()

// Example of a regularly executed service with LaunchParameters
void schedule_functor_launch_parameters()
{
	auto highway =
		hi::make_self_shared<hi::SerialHighWayWithScheduler<>>("SerialHighWay:serial_schedule", nullptr, 10ms, 10ms);

	struct Service
	{
		void operator()(hi::ReschedulableRunnable::LaunchParameters params)
		{
			++starts_;
			scope->print(std::to_string(starts_));
			// Long running algorithm
			const auto start_time = std::chrono::steady_clock::now();
			for (auto cur_time = std::chrono::steady_clock::now(); (cur_time - start_time) < 1s;
				 cur_time = std::chrono::steady_clock::now())
			{
				std::this_thread::sleep_for(50ms);
				if ((params.global_run_id_.get() != params.your_run_id_))
				{
					scope->print("Long running algorithm was aborted");
					break;
				}
			}
			// Schedule next start
			params.schedule_.get().schedule_launch_in(1s);
		}

		const std::shared_ptr<hi::CoutScope> scope{std::make_shared<hi::CoutScope>("Service with LaunchParameters")};
		std::uint32_t starts_{0};
	};

	highway->add_reschedulable_runnable(Service{}, highway->protector_for_tests_only(), __FILE__, __LINE__);

	std::this_thread::sleep_for(15s);

	highway->destroy();
} // schedule_functor_launch_parameters()

int main(int /* argc */, char ** /* argv */)
{
	schedule_simple_runnable();
	schedule_functor();
	schedule_functor_launch_parameters();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
