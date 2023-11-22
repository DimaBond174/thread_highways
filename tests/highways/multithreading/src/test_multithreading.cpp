/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <mutex>
#include <set>

namespace hi
{

using namespace std::chrono_literals;

TEST(TestMultithreading, CountTheNumberOfThreads)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::MultiThreadedTaskProcessingPlant>(16 /* threads */)};

	std::set<std::thread::id> threads_set;
	std::mutex threads_set_protector;
	const auto get_threads_cnt = [&]
	{
		std::lock_guard lg{threads_set_protector};
		const auto re = threads_set.size();
		threads_set.clear();
		return re;
	};

	for (std::int32_t i = 0; i < 1000; ++i)
	{
		highway.object_->try_execute(
			[&]
			{
				std::this_thread::sleep_for(10ms);
				{
					std::lock_guard lg{threads_set_protector};
					threads_set.insert(std::this_thread::get_id());
				}
			});
	}

	std::this_thread::sleep_for(100ms);
	const auto cnt = get_threads_cnt();
	EXPECT_GT(cnt, 2);
}

} // namespace hi
