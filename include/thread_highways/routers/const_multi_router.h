/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_ROUTERS_CONST_MULTI_ROUTER_H
#define THREADS_HIGHWAYS_ROUTERS_CONST_MULTI_ROUTER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/dson/i_has_route_id.h>

#include <map>

namespace hi
{

/**
 * @brief ConstMultiRouter
 * Маршруты подаются в конструкторе.
 * Список константый.
 * Максимальная скорость.
 * @note Одному RouteID может соответствовать несколько маршрутов, отправлено будет всем
 */
class ConstMultiRouter
{
public:
	/**
	 * @brief ConstMultiRouter
	 * Constructor
	 * @param subscriptions
	 * @note usage examples:
	 */
	ConstMultiRouter(
		std::multimap<hi::RouteID, std::shared_ptr<ISubscription<std::shared_ptr<IHasRouteID>>>> subscriptions)
		: subscriptions_{std::move(subscriptions)}
	{
	}

	/**
	 * @brief publish
	 * submit publication
	 * @param publication
	 * @note can be called from any thread
	 * @note usage examples:
	 */
	bool publish(std::shared_ptr<IHasRouteID> publication) const
	{
		if (!publication)
			return false;
		hi::RouteID route_id{};
		if (!publication->get_route_id(route_id))
			return false;
		auto its = subscriptions_.equal_range(route_id);
		bool re{false};
		for (auto it = its.first; it != its.second; ++it)
		{
			if (it->second->send(publication))
			{
				re = true;
			}
		}
		return re;
	}

	bool route_exists(hi::RouteID route_id)
	{
		return subscriptions_.find(route_id) != subscriptions_.end();
	}

private:
	const std::multimap<hi::RouteID, std::shared_ptr<ISubscription<std::shared_ptr<IHasRouteID>>>> subscriptions_;

}; // ConstMultiRouter

} // namespace hi

#endif // THREADS_HIGHWAYS_ROUTERS_CONST_MULTI_ROUTER_H
