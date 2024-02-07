/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_I_HAS_ROUTE_ID_H
#define THREADS_HIGHWAYS_DSON_I_HAS_ROUTE_ID_H

#include <cstdint>

namespace hi
{

using RouteID = std::int64_t;

enum class SystemSignalRoute : std::int64_t
{
    ShutdownSignal = -2,

    LastInEnum = -1, // be shure this last
};

struct IHasRouteID
{
	virtual ~IHasRouteID() = default;

	/**
	 * @brief get_route_id
	 * @param route_id - output route_id
	 * @return if has route_id
	 */
	virtual bool get_route_id(RouteID & route_id) = 0;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_I_HAS_ROUTE_ID_H
