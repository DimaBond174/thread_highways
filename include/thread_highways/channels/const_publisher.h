/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_CONST_PUBLISHER_H
#define THREADS_HIGHWAYS_CHANNELS_CONST_PUBLISHER_H

#include <thread_highways/channels/i_subscription.h>

#include <vector>

namespace hi
{

/**
 * @brief ConstPublisher
 * Подписчики подаются в конструкторе списком.
 * Список константый.
 * Максимальная скорость.
 */
template <typename Publication>
class ConstPublisher
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief ConstPublisher
	 * Constructor
	 * @param subscriptions
	 * @note usage examples:
	 */
	ConstPublisher(std::vector<std::shared_ptr<ISubscription<Publication>>> subscriptions)
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
	void publish(Publication publication) const
	{
		auto size = subscriptions_.size();
		if (!size)
			return;
		--size;
		for (std::size_t i = 0; i < size; ++i)
		{
			subscriptions_[i]->send(publication);
		}
		subscriptions_[size]->send(std::move(publication));
	}

private:
	const std::vector<std::shared_ptr<ISubscription<Publication>>> subscriptions_;

}; // ConstPublisher

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_CONST_PUBLISHER_H
