#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

bool test_std_thread(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	std::condition_variable cv_notify;
	std::mutex messages_mutex;
	std::vector<std::uint32_t> messages;

	auto subscriber_protector = std::make_shared<bool>();
	auto subscriber = [&, protector = std::weak_ptr(subscriber_protector)](std::uint32_t publication) -> bool
	{
		if (auto subscriber_alive = protector.lock())
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
			}
			return true;
		}
		return false;
	};

	std::thread worker(
		[&, subscriber = std::move(subscriber)]
		{
			std::vector<std::uint32_t> messages_local;
			while (complete_future.wait_for(0ms) != std::future_status::ready)
			{
				{
					std::unique_lock<std::mutex> lk(messages_mutex);
					cv_notify.wait(
						lk,
						[&]
						{
							return !messages.empty();
						});
					std::swap(messages_local, messages);
				}

				for (auto && it : messages_local)
				{
					if (!subscriber(it))
						return;
				}
				messages_local.clear();
			}
		});

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		{
			std::lock_guard<std::mutex> slg{messages_mutex};
			messages.emplace_back(msg);
		}
		cv_notify.notify_one();
	}

	worker.join();

	return complete_future.get();
} // test_std_thread

bool test_highway_channel_block_on_wait_holder(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();

	hi::RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};
	auto subscribe_channel = publisher->subscribe_channel();

	hi::subscribe(
		subscribe_channel,
		[&](std::uint32_t publication)
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox(),
		false);

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_block_on_wait_holder

bool test_highway_channel_not_block(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();

	hi::RAIIdestroy highway{hi::make_self_shared<hi::SingleThreadHighWay<>>()};
	highway.object_->set_capacity(burden);
	auto subscribe_channel = publisher->subscribe_channel();

	hi::subscribe(
		subscribe_channel,
		[&](std::uint32_t publication)
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
				return;
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_->mailbox());

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_not_block

bool test_highway_channel_direct_send(const std::uint32_t burden)
{
	std::promise<bool> complete_promise;
	auto complete_future = complete_promise.get_future();

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
	auto subscribe_channel = publisher->subscribe_channel();

	auto subscriber_protector = std::make_shared<bool>();
	hi::subscribe(
		subscribe_channel,
		[&](std::uint32_t publication)
		{
			if (publication == burden)
			{
				complete_promise.set_value(true);
				return;
			}
		},
		std::weak_ptr(subscriber_protector));

	for (std::uint32_t msg = 0; msg <= burden; ++msg)
	{
		publisher->publish(msg);
	}

	return complete_future.get();
} // test_highway_channel_direct_send

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

	funs.emplace_back(TestBundle{test_std_thread, "test_std_thread", 0us});
	funs.emplace_back(
		TestBundle{test_highway_channel_block_on_wait_holder, "test_highway_channel_block_on_wait_holder", 0us});
	funs.emplace_back(TestBundle{test_highway_channel_not_block, "test_highway_channel_not_block", 0us});
	funs.emplace_back(TestBundle{test_highway_channel_direct_send, "test_highway_channel_direct_send", 0us});

	main_test(funs, 10000);
	main_test(funs, 100000);

	std::cout << "Test finished" << std::endl;
	return 0;
}
