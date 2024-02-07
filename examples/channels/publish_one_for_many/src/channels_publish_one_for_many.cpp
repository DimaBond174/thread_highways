#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <atomic>
#include <vector>

extern void scan_files(bool need_to_start_counter);

void channels_publish_one_for_many()
{
	hi::CoutScope scope("channels_publish_one_for_many");

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<int>>();
	auto channel = publisher->subscribe_channel();

	const auto subscribe = [&](hi::HighWayProxyPtr highway)
	{
		return channel->subscribe(
			[&](int publication)
			{
				scope.print(std::to_string(publication));
			},
			highway,
			__FILE__,
			__LINE__);
	};

	std::vector<std::shared_ptr<hi::HighWay>> highways;
	std::vector<std::shared_ptr<hi::ISubscription<int>>> subscriptions;
	for (int i = 0; i < 10; ++i)
	{
		auto it = highways.emplace_back(hi::make_self_shared<hi::HighWay>());
		subscriptions.emplace_back(subscribe(hi::make_proxy(it)));
	}

	for (int i = 0; i < 100; ++i)
	{
		publisher->publish(i);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	for (auto && it : highways)
	{
		it->destroy();
	}
}

struct Counter
{
	Counter()
	{
		print_cnt("Construct", +1);
	}

	Counter(int val)
		: val_{val}
	{
		print_cnt("Construct", +1);
	}

	~Counter()
	{
		print_cnt("Destroy", -1);
	}

	Counter(Counter && other)
	{
		val_ = other.val_;
		print_cnt("Move Construct", +1);
	}

	Counter(const Counter & other)
	{
		val_ = other.val_;
		print_cnt("Copy Construct", +1);
	}

	bool operator==(const Counter & other)
	{
		return val_ == other.val_;
	}

	void print_cnt(const char * what, int val)
	{
		static std::atomic<int> cnt{0};
		cnt += val;
		std::cout << what << ", cnt == " << cnt << std::endl;
	}

	int val_{0};
};

void count_copies_of_the_publication()
{
	hi::CoutScope scope("channels_publish_one_for_many");

	auto publisher = hi::make_self_shared<hi::PublishOneForMany<Counter>>();
	auto channel = publisher->subscribe_channel();
	channel->subscribe(
		[&](const Counter & publication)
		{
			scope.print("Received: " + std::to_string(publication.val_));
		},
		false);

	publisher->publish(Counter{12345});
}

int main(int /* argc */, char ** /* argv */)
{
    //channels_publish_one_for_many();
    //count_copies_of_the_publication();
    scan_files(true);
    scan_files(false);
	std::cout << "Test finished" << std::endl;
	return 0;
}
