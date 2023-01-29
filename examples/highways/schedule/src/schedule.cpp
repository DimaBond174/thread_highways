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
    const auto highway = hi::make_self_shared<hi::HighWay>();

	std::promise<bool> promise;
	auto future = promise.get_future();

    highway->schedule(
        [&, i = 0](hi::Schedule& schedule) mutable
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
        }, std::chrono::steady_clock::now() + 100ms,
		__FILE__,
		__LINE__);

	future.wait();

	highway->destroy();
} // schedule_simple_runnable()

// Example of a regularly executed service
void schedule_functor()
{
    const auto highway = hi::make_self_shared<hi::HighWay>();

	struct Service
	{
        void operator()(hi::Schedule& schedule)
		{
			++starts_;
			scope->print(std::to_string(starts_));
			// Schedule next start
			schedule.schedule_launch_in(50ms);
		}

		const std::shared_ptr<hi::CoutScope> scope{std::make_shared<hi::CoutScope>("Service")};
		std::uint32_t starts_{0};
	};

    highway->schedule(Service{}, {}, __FILE__, __LINE__);
	std::this_thread::sleep_for(1s);

	highway->destroy();
} // schedule_functor()

int main(int /* argc */, char ** /* argv */)
{
	schedule_simple_runnable();
	schedule_functor();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
