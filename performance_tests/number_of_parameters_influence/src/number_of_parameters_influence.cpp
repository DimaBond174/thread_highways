#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

std::shared_ptr<hi::HighWay> & pre_heated_highway()
{
	static hi::RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};
	return highway.object_;
}

bool test_highway_channel_block_on_wait_holder(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	const auto subscribe_channel = publisher->subscribe_channel();

	const auto highway = pre_heated_highway();

	auto subsciption = publisher->subscribe_channel()->subscribe(
		[&](std::uint32_t publication)
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway,
		__FILE__,
		__LINE__,
		false,
		false);

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_block_on_wait_holder

bool test_highway_channel_block_on_wait_holder_with_keep_execution_control(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	const auto subscribe_channel = publisher->subscribe_channel();

	const auto highway = pre_heated_highway();

	auto subsciption = publisher->subscribe_channel()->subscribe(
		[&](std::uint32_t publication, const std::atomic<bool> & keep_execution)
		{
			if (publication == burden || !keep_execution.load())
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway,
		__FILE__,
		__LINE__,
		false,
		false);

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_block_on_wait_holder_with_run_id_control

bool test_highway_channel_not_block(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	const auto subscribe_channel = publisher->subscribe_channel();

	const auto highway = pre_heated_highway();

	auto subsciption = publisher->subscribe_channel()->subscribe(
		[&](std::uint32_t publication)
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway,
		__FILE__,
		__LINE__);

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_not_block

bool test_highway_channel_not_block_with_keep_execution_control(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	const auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	const auto subscribe_channel = publisher->subscribe_channel();

	const auto highway = pre_heated_highway();

	auto subsciption = publisher->subscribe_channel()->subscribe(
		[&](std::uint32_t publication, const std::atomic<bool> & keep_execution)
		{
			if (publication == burden || !keep_execution.load())
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway,
		__FILE__,
		__LINE__);

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_not_block_with_run_id_control

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

/*
  This load test examines the impact of the number of parameters passed to the callback on performance.
*/
int main(int /* argc */, char ** /* argv */)
{
	pre_heated_highway()->set_capacity(100000);
	test_highway_channel_block_on_wait_holder(100000);

	std::vector<TestBundle> funs;

	funs.emplace_back(
		TestBundle{test_highway_channel_block_on_wait_holder, "test_highway_channel_block_on_wait_holder", 0us});
	funs.emplace_back(TestBundle{
		test_highway_channel_block_on_wait_holder_with_keep_execution_control,
		"test_highway_channel_block_on_wait_holder_with_keep_execution_control",
		0us});
	funs.emplace_back(TestBundle{test_highway_channel_not_block, "test_highway_channel_not_block", 0us});
	funs.emplace_back(TestBundle{
		test_highway_channel_not_block_with_keep_execution_control,
		"test_highway_channel_not_block_with_keep_execution_control",
		0us});

	main_test(funs, 10000);
	main_test(funs, 100000);

	std::cout << "Test finished" << std::endl;
	return 0;
}
