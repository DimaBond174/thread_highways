/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_ROUTERS_CONST_ROUTER_H
#define THREADS_HIGHWAYS_ROUTERS_CONST_ROUTER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/dson/i_has_route_id.h>

#include <map>

namespace hi
{

/**
 * @brief ConstRouter
 * Маршруты подаются в конструкторе.
 * Список константый.
 * Максимальная скорость.
 * @note Одному RouteID может соответствовать только один маршрут
 */
class ConstRouter
{
public:
	/**
	 * @brief ConstRouter
	 * Constructor
	 * @param subscriptions
	 * @note usage examples:
	 */
	ConstRouter(std::map<hi::RouteID, std::shared_ptr<ISubscription<std::unique_ptr<IHasRouteID>>>> subscriptions)
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
	bool publish(std::unique_ptr<IHasRouteID> publication) const
	{
		if (!publication)
			return false;
		hi::RouteID route_id{};
		if (!publication->get_route_id(route_id))
			return false;
		auto found = subscriptions_.find(route_id);
		if (found == subscriptions_.end())
			return false;
		return found->second->send(std::move(publication));
	}

	bool route_exists(hi::RouteID route_id)
	{
		return subscriptions_.find(route_id) != subscriptions_.end();
	}

private:
	const std::map<hi::RouteID, std::shared_ptr<ISubscription<std::unique_ptr<IHasRouteID>>>> subscriptions_;

}; // ConstRouter

} // namespace hi

#endif // THREADS_HIGHWAYS_ROUTERS_CONST_ROUTER_H
