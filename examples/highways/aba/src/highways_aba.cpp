#include <thread_highways/include_all.h>
#include <thread_highways/highways/highway_aba_safe.h>
#include <thread_highways/tools/cout_scope.h>

#include <future>
#include <vector>

template <typename H>
void aba_test(H highway, const size_t load_size, const size_t threads_cnt)
{
	hi::CoutScope scope(
		"aba_test(), load_size=" + std::to_string(load_size) + ", threads_cnt=" + std::to_string(threads_cnt));

	const auto aba_load = [&](std::size_t thread_id)
	{
		std::vector<std::future<std::size_t>> futures;
		futures.reserve(load_size);
		for (std::size_t i = 0u; i < load_size; ++i)
		{
			std::promise<std::size_t> prom;

			futures.emplace_back(prom.get_future());
			highway->execute(
				[&, i, p = std::move(prom), executed = bool{false}]() mutable
				{
					if (executed)
					{
						scope.print(
							"SECOND exec highway task: thread_id=" + std::to_string(thread_id)
							+ ", i * i =" + std::to_string(i * i));
						return;
					}
					const auto res = i * i;
					scope.print(
						"highway task: thread_id=" + std::to_string(thread_id) + ", i * i =" + std::to_string(res));
					p.set_value(res);
					executed = true;
				});
		}

		for (std::size_t i = 0u; i < load_size; ++i)
		{
			// Если упадёт с what():  std::future_error: Broken promise
			// значит лямбду с std::promise удалили без запуска.
			const auto expected = i * i;
			std::string info =
				"task Result from: thread_id=" + std::to_string(thread_id) + ", i*i=" + std::to_string(expected);
			scope.print(info);
			try
			{
				auto res = futures[i].get();
				scope.print(info + ", future = " + std::to_string(res));
				assert(res == i * i);
			}
			catch (std::exception e)
			{
				scope.print(info + ", error:" + e.what());
				std::cout << std::flush;
				throw 12345;
			}
		}
	};

	std::vector<std::thread> threads;
	for (std::size_t i = 0u; i < threads_cnt; ++i)
	{
		threads.emplace_back(
			[&, i]
			{
				aba_load(i);
			});
	}

	for (std::size_t i = 0u; i < threads_cnt; ++i)
	{
		threads[i].join();
	}

	// Stop threads
	highway->destroy();
}

int main(int /* argc */, char ** /* argv */)
{
	{
		// Create executor
		const auto highway = hi::make_self_shared<hi::HighWayAbaSafe>(100 /*mail_box_capacity*/);
		// aba_test(highway, 10000, 1);
		// aba_test(100, 1);
		// aba_test(highway, 100, 10);
		// aba_test(highway, 1000, 10);
		aba_test(highway, 1000, 40);
	}

	{
		// Create executor
		//    const auto highway = hi::make_self_shared<hi::HighWay>();
		// aba_test(100, 1);
		//    aba_test(highway, 100, 20);
		// aba_test(1000, 10);
	}

	std::cout << "Tests finished" << std::endl;
	return 0;
}
