#include <thread_highways/include_all.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

using namespace std::chrono_literals;

void serial_schedule()
{
	auto logger = hi::create_default_logger(
		[](const std::string & err)
		{
			std::cout << err << std::endl;
		});
	auto highway = hi::make_self_shared<hi::SerialHighWayWithScheduler<>>(
		"SerialHighWay:serial_schedule",
		std::move(logger),
		std::chrono::milliseconds{10},
		std::chrono::milliseconds{10});

	hi::HighWaysMonitoring monitoring{100ms};
	monitoring.add_for_monitoring(highway);

	std::promise<bool> promise;
	auto future = promise.get_future();
	highway->add_reschedulable_runnable(hi::ReschedulableRunnable::create(
		[&, i = 0](
			hi::ReschedulableRunnable::Schedule & schedule,
			const std::atomic<std::uint32_t> &,
			const std::uint32_t) mutable
		{
			std::cout << "number of launches:" << ++i << std::endl;
			std::this_thread::sleep_for(100ms);
			if (i < 10)
			{
				schedule.schedule_launch_in(100ms);
			}
			else
			{
				promise.set_value(true);
			}
		},
		__FILE__,
		__LINE__));

	future.wait();

	highway->destroy();
} // serial_schedule()

int main(int /* argc */, char ** /* argv */)
{
	serial_schedule();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
