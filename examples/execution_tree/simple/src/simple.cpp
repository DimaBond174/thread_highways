#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

// The functor must implement operator() with one of the possible interfaces
struct ScienceOfNotMovable
{
	ScienceOfNotMovable(hi::CoutScope & scope)
		: scope_{scope}
	{
		scope_.print("Science constructed");
	}

	void operator()(int data, const std::int32_t /*label*/, hi::Publisher<std::string> & publisher)
	{
		scope_.print("Science calculations");
		science_data_->append(std::to_string(data) + ", ");
		publisher.publish_on_highway(*science_data_);
	}

	~ScienceOfNotMovable()
	{
		std::lock_guard lg{mutex_}; // just using for using
		scope_.print(std::string{"~Science data: "}.append(*science_data_));
	}
	ScienceOfNotMovable(const ScienceOfNotMovable &) = delete;
	ScienceOfNotMovable & operator=(const ScienceOfNotMovable &) = delete;
	ScienceOfNotMovable(ScienceOfNotMovable &&) = delete;
	ScienceOfNotMovable & operator=(ScienceOfNotMovable &&) = delete;

	hi::CoutScope & scope_;
	std::unique_ptr<std::string> science_data_;

	// Just to demonstrate the movement of what cannot be moved
	std::mutex mutex_;
};

// An example of sending some calculations to an executor
void future_node()
{
	hi::CoutScope scope("future_node()");
	const auto highway = hi::make_self_shared<hi::HighWay>();

	auto science = std::make_unique<ScienceOfNotMovable>(scope);
	// Preparation
	science->science_data_ = std::make_unique<std::string>("research results: ");

	// Future
	// We wrap into unique_ptr the functor that has the mutex so that it can be moved
	auto future_node =
		hi::make_self_shared<hi::DefaultNode<int, std::string, decltype(science)>>(std::move(science), highway, 1);

	// Subscribe fo results
	auto subscription = hi::create_subscription_for_new_only<std::string>(
		[&](std::string result)
		{
			scope.print(std::string{"Received future result:"}.append(result));
		});
	future_node->subscribe_direct(subscription, future_node->get_unique_label());

	// Send a signal to start
	future_node->send_param_in_highway_channel(1);
	future_node->send_param_in_highway_channel(2);
	future_node->send_param_in_highway_channel(3);

	highway->flush_tasks();
	highway->destroy();
} // future_node()

void future_node2()
{
	hi::CoutScope scope("future_node2()");
	const auto highway = hi::make_self_shared<hi::HighWay>();

	auto science = std::make_shared<ScienceOfNotMovable>(scope);
	// Preparation
	science->science_data_ = std::make_unique<std::string>("research results: ");

	// Future
	// We wrap into unique_ptr the functor that has the mutex so that it can be moved
	auto future_node =
		hi::ExecutionTreeDefaultNodeFabric<int, std::string>::create(std::move(science), hi::make_proxy(highway));

	// Subscribe fo results
	auto subscription = hi::create_subscription_for_new_only<std::string>(
		[&](std::string result)
		{
			scope.print(std::string{"Received future result:"}.append(result));
		});
	future_node->subscribe_direct(subscription, future_node->get_unique_label());

	// Send a signal to start
	future_node->send_param_in_highway_channel(1);
	future_node->send_param_in_highway_channel(2);
	future_node->send_param_in_highway_channel(3);

	highway->flush_tasks();
	highway->destroy();
} // future_node2()

// An example using the full callback interface
void future_node_show_progress()
{
	hi::CoutScope scope("future_node()");

	// Result
	auto result_subscription = hi::create_subscription<std::string>(
		[&](std::string result)
		{
			scope.print(result);
		});

	const auto highway1 = hi::make_self_shared<hi::HighWay>();
	highway1->execute(
		[]()
		{
			hi::set_this_thread_name("highway1");
		},
		__FILE__,
		__LINE__);

	const auto highway2 = hi::make_self_shared<hi::HighWay>();
	highway2->execute(
		[]()
		{
			hi::set_this_thread_name("highway2");
		},
		__FILE__,
		__LINE__);

	hi::ExecutionTreeWithProgressPublisher execution_tree{highway1};

	{
		// Future
		auto future_node = hi::ExecutionTreeDefaultNodeFabric<int, std::string>::create(
			[&, progress = std::int32_t{0}](
				int /*publication*/,
				const std::int32_t /*label*/,
				hi::Publisher<std::string> & publisher,
				const std::atomic<bool> & keep_execution) mutable
			{
				scope.print("future_node started on " + hi::get_this_thread_name());
				while (keep_execution)
				{
					progress += 1;
					publisher.progress(progress);
					std::this_thread::sleep_for(std::chrono::milliseconds{10});
				}
				return std::string{"result progress = "}.append(std::to_string(progress));
			},
			execution_tree,
			hi::make_proxy(highway2),
			execution_tree.generate_node_id());

		future_node->subscribe_direct(result_subscription);
	} // scope

	auto subscriber = execution_tree.subscribe_channel()->subscribe(
		[&](hi::NodeProgress progress)
		{
			scope.print("progress received on " + hi::get_this_thread_name());
			scope.print(
				std::string{"node №"} + std::to_string(progress.node_id_)
				+ " achived progress: " + std::to_string(progress.achieved_progress_));
		},
		hi::make_proxy(highway1),
		__FILE__,
		__LINE__);

	// Send a signal to start
	execution_tree.start(1);

	std::this_thread::sleep_for(std::chrono::milliseconds{100});

	highway2->destroy();
	highway1->destroy();
} // future_node_show_progress()

int main(int /* argc */, char ** /* argv */)
{
	future_node();
	future_node2();
	future_node_show_progress();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
