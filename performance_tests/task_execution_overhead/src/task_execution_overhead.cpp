#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

bool test_std_future(const std::uint32_t burden)
{
	std::atomic<std::uint32_t> counter{0};
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();
	std::vector<std::future<std::uint32_t>> tasks;
	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		tasks.emplace_back(std::async(
			std::launch::async,
			[&, i]
			{
				const auto cur = counter.fetch_add(1);
				if (cur == burden)
				{
					complete_promise.set_value(true);
				}
				return i + i;
			}));
	}
	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		if (tasks[i].get() / 2 != i)
		{
			return false;
		}
	}
	return complete_future.get();
} // test_std_future

bool test_std_thread_v1(const std::uint32_t burden)
{
	std::atomic<std::uint32_t> counter{0};
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	std::mutex tasks_mutex;
	std::condition_variable cv;
	std::vector<std::function<void()>> tasks;

	std::thread worker_thread(
		[&]
		{
			std::unique_lock<std::mutex> lk(tasks_mutex);
			while (complete_future.wait_for(0ms) != std::future_status::ready)
			{
				cv.wait(
					lk,
					[&]
					{
						return !tasks.empty();
					});
				for (auto && it : tasks)
				{
					it();
				}
				tasks.clear();
			}
		});

	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		{
			std::lock_guard lg{tasks_mutex};
			tasks.emplace_back(
				[&, i]
				{
					const auto cur = counter.fetch_add(1);
					if (cur == burden)
					{
						complete_promise.set_value(true);
					}
				});
		}

		cv.notify_one();
	}

	worker_thread.join();
	return complete_future.get();
} // test_std_thread_v1

bool test_std_thread_v2(const std::uint32_t burden)
{
	std::atomic<std::uint32_t> counter{0};
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	std::mutex tasks_mutex;
	std::condition_variable cv;
	std::vector<std::function<void()>> tasks;

	std::thread worker_thread(
		[&]
		{
			std::vector<std::function<void()>> tasks_local;
			while (complete_future.wait_for(0ms) != std::future_status::ready)
			{
				{
					std::unique_lock<std::mutex> lk(tasks_mutex);
					cv.wait(
						lk,
						[&]
						{
							return !tasks.empty();
						});
					std::swap(tasks_local, tasks);
				}

				for (auto && it : tasks_local)
				{
					it();
				}
				tasks_local.clear();
			}
		});

	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		{
			std::lock_guard lg{tasks_mutex};
			tasks.emplace_back(
				[&, i]
				{
					const auto cur = counter.fetch_add(1);
					if (cur == burden)
					{
						complete_promise.set_value(true);
					}
				});
		}

		cv.notify_one();
	}

	worker_thread.join();
	return complete_future.get();
} // test_std_thread_v2

/*
	In this test, the amount of holders is limited by default
	and in case of shortage there will be a blocking wait on the semaphore.
*/
bool test_highway_block_on_wait_holder(const std::uint32_t burden)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};

	std::atomic<std::uint32_t> counter{0};
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		highway.object_->post(
			[&]
			{
				const auto cur = counter.fetch_add(1);
				if (cur == burden)
				{
					complete_promise.set_value(true);
				}
			},
			false);
	}

	return complete_future.get();
} // test_highway_block_on_wait_holder

/*
	In this test, the number of holders is not limited, work without blocking.
*/
bool test_highway_not_block(const std::uint32_t burden)
{
	hi::RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};
	highway.object_->set_capacity(burden);

	std::atomic<std::uint32_t> counter{0};
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	for (std::uint32_t i = 0; i <= burden; ++i)
	{
		highway.object_->post(
			[&]
			{
				const auto cur = counter.fetch_add(1);
				if (cur == burden)
				{
					complete_promise.set_value(true);
				}
			});
	}

	return complete_future.get();
} // test_highway_not_block

struct TestBundle
{
	std::function<bool(const std::uint32_t)> fun;
	std::string fun_name;
	std::chrono::microseconds execution_time;
};

void main_test(std::vector<TestBundle> & funs, const std::uint32_t burden)
{
	hi::CoutScope scope(std::string{"Start main_test for burden="}.append(std::to_string(burden)));
	const std::uint32_t avg_times{10};
	for (std::uint32_t i = 0; i < avg_times; ++i)
	{
		for (auto && it : funs)
		{
			const auto start = std::chrono::steady_clock::now();
			if (!it.fun(burden))
				return;
			const auto finish = std::chrono::steady_clock::now();
			it.execution_time += std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
		}
	}

	for (auto && it : funs)
	{
		scope.print(std::string{it.fun_name}
						.append(" execution_time microsec per 100 tasks: ")
						.append(std::to_string((it.execution_time / (avg_times * burden / 100)).count())));
		it.execution_time = 0us;
	}
}

int main(int /* argc */, char ** /* argv */)
{
	std::vector<TestBundle> funs;

	funs.emplace_back(TestBundle{test_std_thread_v1, "test_std_thread_v1", 0us});
	funs.emplace_back(TestBundle{test_std_thread_v2, "test_std_thread_v2", 0us});
	funs.emplace_back(TestBundle{test_highway_block_on_wait_holder, "test_highway_block_on_wait_holder", 0us});
	funs.emplace_back(TestBundle{test_highway_not_block, "test_highway_not_block", 0us});

	main_test(funs, 100000);
	main_test(funs, 10000);

	// Don't even try to use std::future for heavy workloads
	funs.emplace_back(TestBundle{test_std_future, "test_std_future", 0us});
	main_test(funs, 1000);

	std::cout << "Test finished" << std::endl;
	return 0;
}
