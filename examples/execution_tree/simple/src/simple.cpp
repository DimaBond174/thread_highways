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

void test_1()
{
	//	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	//	auto highway_mailbox = highway->mailbox();

	//	auto future_node_logic = hi::FutureNodeLogic<std::int32_t, std::int32_t>::create(
	//		[&](std::int32_t parameter, hi::IPublisher<std::int32_t> & result_publisher)
	//		{
	//			assert(highway->current_execution_on_this_highway());
	//			// логика преобразования входящих параметров в результат
	//			result_publisher.publish(parameter * parameter);
	//		},
	//		highway->protector(),
	//		__FILE__,
	//		__LINE__);

	//	auto future_node =
	//		hi::make_self_shared<hi::FutureNode<std::int32_t, std::int32_t>>(std::move(future_node_logic),
	// highway_mailbox);

	//	std::promise<bool> future_result;
	//	auto future_result_ready = future_result.get_future();
	//	auto subscription_callback = hi::SubscriptionCallback<std::int32_t>::create(
	//		[&](std::int32_t publication, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
	//		{
	//			std::cout << "future result = " << publication << std::endl;
	//			future_result.set_value(true);
	//		},
	//		highway->protector(),
	//		__FILE__,
	//		__LINE__);

	//	std::cout << "subscription_callback from:" << subscription_callback->get_code_filename()
	//			  << ", string:" << subscription_callback->get_code_line() << std::endl;

	//	future_node->result_channel()->subscribe(
	//		hi::Subscription<std::int32_t>::create(std::move(subscription_callback), highway_mailbox));

	//	// Прямой запуск фьючи
	//	future_node->subscription().send(3);
	//	if (!future_result_ready.get())
	//		return;

	//	future_result = {};
	//	future_result_ready = future_result.get_future();

	//	// Запуск фьючи через подписку к другому узлу
	//	hi::PublishManyForOne<std::int32_t> start_calculations_publisher{future_node->subscription()};
	//	start_calculations_publisher.publish(5);
	//	if (!future_result_ready.get())
	//		return;

} // test_1

/*
  Тест кейс: получение offline и online маршрута.
  Online имеет приоритет - если он пришел первым, то его и отдаём как результат
  Offline как заглушка если Online тормозит или не смог - если Online таки подготовится,
	то Online маршрут отправляем даже если уже отправляли Offline
*/
void test_2_offline_online_route()
{
	//	struct FutureSendOnlineOrOffline
	//	{
	//		FutureSendOnlineOrOffline(
	//			std::weak_ptr<FutureSendOnlineOrOffline> self_weak,
	//			hi::IHighWayMailBoxPtr high_way_mail_box)
	//			: self_weak_{std::move(self_weak)}
	//			, in_channel_for_online_route_{hi::Subscription<std::string>::create(
	//				  hi::SubscriptionCallback<std::string>::create(
	//					  &FutureSendOnlineOrOffline::on_online_route,
	//					  self_weak_,
	//					  __FILE__,
	//					  __LINE__),
	//				  high_way_mail_box)}
	//			, in_channel_for_offline_route_{hi::Subscription<std::string>::create(
	//				  hi::SubscriptionCallback<std::string>::create(
	//					  &FutureSendOnlineOrOffline::on_offline_route,
	//					  self_weak_,
	//					  __FILE__,
	//					  __LINE__),
	//				  high_way_mail_box)}
	//			, in_channel_for_reset_{hi::Subscription<bool>::create(
	//				  hi::SubscriptionCallback<bool>::create(
	//					  &FutureSendOnlineOrOffline::reset,
	//					  self_weak_,
	//					  __FILE__,
	//					  __LINE__),
	//				  high_way_mail_box)}
	//			, out_future_result_publisher_{hi::make_self_shared<hi::PublishOneForMany<std::string>>()}
	//		{
	//		}

	//		void on_online_route(std::string route, const std::atomic<std::uint32_t> &, const std::uint32_t)
	//		{
	//			online_route_was_sent_ = true;
	//			out_future_result_publisher_->publish(std::move(route));
	//		}

	//		void on_offline_route(std::string route, const std::atomic<std::uint32_t> &, const std::uint32_t)
	//		{
	//			if (online_route_was_sent_)
	//				return;
	//			out_future_result_publisher_->publish(std::move(route));
	//		}

	//		void reset(bool, const std::atomic<std::uint32_t> &, const std::uint32_t)
	//		{
	//			online_route_was_sent_ = false;
	//		}

	//		const std::weak_ptr<FutureSendOnlineOrOffline> self_weak_;
	//		const hi::PublishManyForOne<std::string> in_channel_for_online_route_;
	//		const hi::PublishManyForOne<std::string> in_channel_for_offline_route_;
	//		const hi::PublishManyForOne<bool> in_channel_for_reset_;
	//		const std::shared_ptr<hi::PublishOneForMany<std::string>> out_future_result_publisher_;
	//		bool online_route_was_sent_{false};
	//	}; // FutureSendOnlineOrOffline

	//	auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
	//	auto highway_mailbox = highway->mailbox();
	//	auto route_switch_node = hi::make_self_shared<FutureSendOnlineOrOffline>(highway_mailbox);

	//	route_switch_node->out_future_result_publisher_->subscribe(hi::Subscription<std::string>::create(
	//		hi::SubscriptionCallback<std::string>::create(
	//			[&](std::string route, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
	//			{
	//				std::cout << "future result route = " << route << std::endl;
	//			},
	//			highway->protector_for_tests_only(),
	//			__FILE__,
	//			__LINE__),
	//		highway_mailbox));

	//	{
	//		std::cout << "case1: first online was prepared:" << std::endl;
	//		route_switch_node->in_channel_for_online_route_.publish("online");
	//		route_switch_node->in_channel_for_offline_route_.publish("offline");
	//		route_switch_node->in_channel_for_reset_.publish(true);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//	}

	//	{
	//		std::cout << "case2: first offline was prepared:" << std::endl;
	//		route_switch_node->in_channel_for_offline_route_.publish("offline");
	//		route_switch_node->in_channel_for_online_route_.publish("online");
	//		route_switch_node->in_channel_for_reset_.publish(true);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//	}

	//	highway->flush_tasks();
} // test_2_offline_online_route()

int main(int /* argc */, char ** /* argv */)
{
	void_future_node();
	void_future_node_full_callback();
	test_1();
	test_2_offline_online_route();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
