#include <thread_highways/include_all.h>

#include <cassert>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

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
	//highway_mailbox);

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
	test_1();
	test_2_offline_online_route();

	std::cout << "Tests finished" << std::endl;

	return 0;
}
