#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <mutex>
#include <set>

using namespace std::chrono_literals;

int main(int /* argc */, char ** /* argv */)
{
	hi::CoutScope scope("Example of scaling and shrinking threads");

	hi::RAIIdestroy highway{hi::make_self_shared<hi::ConcurrentHighWay<>>(
		"HighWay",
		nullptr,
		10ms, // max_task_execution_time
		1ms // workers_change_period
		)};

	highway.object_->set_max_concurrent_workers(8);

	std::set<std::thread::id> threads_set;
	std::mutex threads_set_protector;
	const auto get_threads_cnt = [&]
	{
		std::lock_guard lg{threads_set_protector};
		const auto re = threads_set.size();
		threads_set.clear();
		return re;
	};

	scope.print("Workload started\n================\n");
	for (std::int32_t i = 0; i < 1000; ++i)
	{
		highway.object_->post(
			[&]
			{
				{
					std::lock_guard lg{threads_set_protector};
					threads_set.insert(std::this_thread::get_id());
				}
				// Workload - heavy task for 10ms
				std::this_thread::sleep_for(10ms);
			});
		if (0 == i % 10)
		{
			scope.print(std::string{"get_threads_cnt="}.append(std::to_string(get_threads_cnt())));
		}
		std::this_thread::sleep_for(1ms);
	}

	scope.print("Workload stopped\n================\n");
	for (std::int32_t i = 0; i < 1000; ++i)
	{
		highway.object_->post(
			[&]
			{
				{
					std::lock_guard lg{threads_set_protector};
					threads_set.insert(std::this_thread::get_id());
				}
			});
		if (0 == i % 10)
		{
			scope.print(std::string{"get_threads_cnt="}.append(std::to_string(get_threads_cnt())));
		}
		std::this_thread::sleep_for(3ms);
	}

	std::cout << "Tests finished" << std::endl;
	return 0;
}
