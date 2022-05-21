#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

using namespace std::chrono_literals;

// The functor must implement operator() with one of the possible interfaces
struct ScienceOfNotMovable
{
	ScienceOfNotMovable(hi::CoutScope & scope)
		: scope_{scope}
		, science_data_{std::make_unique<std::string>("12345")}
	{
		scope_.print("Science constructed");
	}

	void operator()(hi::IPublisher<std::string> & result_publisher)
	{
		scope_.print("Science calculations");
		result_publisher.publish(std::string{"Scientific discovery: "}.append(*science_data_));
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
void void_future_node()
{
	hi::CoutScope scope("future_node()");
	const auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();

	// Future
	// We wrap into unique_ptr the functor that has the mutex so that it can be moved
	auto future_node = hi::VoidFutureNode<std::string>::create(
		std::make_unique<ScienceOfNotMovable>(scope),
		highway->protector_for_tests_only(),
		highway);

	// Subscribe fo results
	hi::subscribe(
		future_node->result_channel(),
		[&](std::string publication)
		{
			scope.print(std::string{"Received future result:"}.append(publication));
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

	// Send a signal to start
	future_node->execute();

	highway->flush_tasks();
	highway->destroy();
} // void_future_node()

// An example using the full callback interface
void void_future_node_full_callback()
{
	hi::CoutScope scope("future_node()");
	const auto highway = hi::make_self_shared<hi::ConcurrentHighWay<>>();

	hi::ExecutionTree execution_tree;

	// Future
	auto future_node = hi::VoidFutureNode<std::string>::create(
		[&](hi::IPublisher<std::string> & result_publisher,
			hi::INode & node,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id)
		{
			if (your_run_id != global_run_id)
				return;
			scope.print(std::string{"Executed future_node №"}.append(std::to_string(node.node_id())));
			node.publish_progress_state(true, 99);
			result_publisher.publish("Scientific discovery: you can publish which ExecutionTree node is currently "
									 "executing and what % of progress is made");
		},
		highway->protector_for_tests_only(),
		highway,
		__FILE__,
		__LINE__,
		execution_tree.current_executed_node_publisher(),
		555);

	// Subscribe for result
	hi::subscribe(
		future_node->result_channel(),
		[&](std::string publication)
		{
			scope.print(std::string{"Received future result:"}.append(publication));
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

	// Subscribe for progress
	execution_tree.current_executed_node_publisher()->subscribe(
		[&](hi::CurrentExecutedNode progress)
		{
			std::string str{"execution_tree: "};
			if (progress.in_progress_)
			{
				str.append("now executing node №");
			}
			else
			{
				str.append("finished node №");
			}
			str.append(std::to_string(progress.node_id_))
				.append(", progress achived: ")
				.append(std::to_string(progress.achieved_progress_));

			scope.print(std::move(str));
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

	// Send a signal to start
	future_node->execute();

	highway->flush_tasks();
	highway->destroy();
} // void_future_node_full_callback()

int main(int /* argc */, char ** /* argv */)
{
	void_future_node();
	void_future_node_full_callback();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
