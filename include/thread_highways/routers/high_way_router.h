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
*/
class HighWayRouter
{
public:
	HighWayRouter(std::weak_ptr<HighWayRouter> self_weak, HighWayProxyPtr highway)
		: self_weak_{std::move(self_weak)}
		, highway_{std::move(highway)}
	{
	}

	HighWayRouter(std::weak_ptr<HighWayRouter> self_weak, std::shared_ptr<HighWay> highway)
		: self_weak_{std::move(self_weak)}
		, highway_{make_proxy(std::move(highway))}
	{
	}

	void publish(std::unique_ptr<IHasRouteID> publication)
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
	void subscribe(hi::RouteID route_id, std::weak_ptr<ISubscription<std::unique_ptr<IHasRouteID>>> subscription)
	{
		highway_->execute(
			[&, route_id, subscription = std::move(subscription), weak = self_weak_]() mutable
			{
				if (auto alive = weak.lock())
				{
					subscriptions_.insert_or_assign(route_id, std::move(subscription));
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);
	}

private:
	void publish_impl(std::unique_ptr<IHasRouteID> publication)
	{
		if (!publication)
			return;
		hi::RouteID route_id{0};
		// if (!publication->get_route_id(route_id)) return;
		publication->get_route_id(route_id); // 0 is Ok
		auto found = subscriptions_.find(route_id);
		if (found == subscriptions_.end())
			return;
		if (auto alive = found->second.lock())
		{
			if (!alive->send(std::move(publication)))
			{
				subscriptions_.erase(found);
			}
		}
		else
		{
			subscriptions_.erase(found);
		}
	}

private:
	const std::weak_ptr<HighWayRouter> self_weak_;
	const HighWayProxyPtr highway_;

	std::map<hi::RouteID, std::weak_ptr<ISubscription<std::unique_ptr<IHasRouteID>>>> subscriptions_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_ROUTERS_HIGHWAY_ROUTER_H
