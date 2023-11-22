#ifndef THREADS_HIGHWAYS_ROUTERS_HIGHWAY_ROUTER_H
#define THREADS_HIGHWAYS_ROUTERS_HIGHWAY_ROUTER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/dson/i_has_route_id.h>
#include <thread_highways/highways/highway.h>

#include <map>

namespace hi
{

/*
	Подписки и публикации перепланируются на хайвей который под капотом -
	поэтому нет гонок на многопоточке.
	Бонус: подписчики могут не перепланировать доставку на свой поток - использовать прямую доставку
	полагаясь что вызов будет в однопоточке highway
	Отличие от HighWayRouter в том что на каждый RouteID несколько маршрутов
	 + объект в shared_ptr
*/
class HighWayMultiRouter
{
public:
	HighWayMultiRouter(std::weak_ptr<HighWayMultiRouter> self_weak, HighWayProxyPtr highway)
		: self_weak_{std::move(self_weak)}
		, highway_{std::move(highway)}
	{
	}

	HighWayMultiRouter(std::weak_ptr<HighWayMultiRouter> self_weak, std::shared_ptr<HighWay> highway)
		: self_weak_{std::move(self_weak)}
		, highway_{make_proxy(std::move(highway))}
	{
	}

	void publish(std::shared_ptr<IHasRouteID> publication)
	{
		highway_->execute(
			[&, publication = std::move(publication), weak = self_weak_]() mutable
			{
				if (auto alive = weak.lock())
				{
					publish_impl(std::move(publication));
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);
	}

	/**
	 * @brief subscribe
	 * saving a subscription
	 * @param subscription
	 */
	void subscribe(hi::RouteID route_id, std::weak_ptr<ISubscription<std::shared_ptr<IHasRouteID>>> subscription)
	{
		highway_->execute(
			[&, route_id, subscription = std::move(subscription), weak = self_weak_]() mutable
			{
				if (auto alive = weak.lock())
				{
					subscriptions_.emplace(route_id, std::move(subscription));
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);
	}

private:
	void publish_impl(std::shared_ptr<IHasRouteID> publication)
	{
		if (!publication)
			return;
		hi::RouteID route_id{};
		if (!publication->get_route_id(route_id))
			return;
		auto its = subscriptions_.equal_range(route_id);
		for (auto it = its.first; it != its.second;)
		{
			if (auto alive = it->second.lock())
			{
				if (alive->send(std::move(publication)))
				{
					++it;
				}
				else
				{
					subscriptions_.erase(it);
				}
			}
			else
			{
				subscriptions_.erase(it);
			}
		}
	}

private:
	const std::weak_ptr<HighWayMultiRouter> self_weak_;
	const HighWayProxyPtr highway_;

	std::multimap<hi::RouteID, std::weak_ptr<ISubscription<std::shared_ptr<IHasRouteID>>>> subscriptions_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_ROUTERS_HIGHWAY_ROUTER_H
