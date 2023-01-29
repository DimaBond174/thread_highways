#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <mutex>
#include <set>

using namespace std::chrono_literals;

int main(int /* argc */, char ** /* argv */)
{
    hi::CoutScope scope("Multy thread example");

        hi::RAIIdestroy highway{hi::make_self_shared<hi::MultiThreadedTaskProcessingPlant>(
            16 /* threads */,
            [&](const hi::Exception& ex)
            {
                scope.print(ex.what_as_string() + " in " + ex.file_name() + ": " + std::to_string(ex.file_line()));
            } /* Exceptions handler */,
            "HighWay with time control" /* highway name*/,
            std::chrono::milliseconds{1} /* max task execution time */,
            500 /* mail box capacity */)};

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
        highway.object_->try_execute(
			[&]
			{
                std::lock_guard lg{threads_set_protector};
                threads_set.insert(std::this_thread::get_id());
			});
	}

    scope.print(std::string{"get_threads_cnt="}.append(std::to_string(get_threads_cnt())));
    std::this_thread::sleep_for(100ms);
    scope.print(std::string{"get_threads_cnt="}.append(std::to_string(get_threads_cnt())));

	return 0;
}
