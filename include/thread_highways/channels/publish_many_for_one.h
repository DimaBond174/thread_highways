/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_PUBLISH_MANY_FOR_ONE_H
#define THREADS_HIGHWAYS_CHANNELS_PUBLISH_MANY_FOR_ONE_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/highways/highway.h>

namespace hi
{

/**
 * @brief PublishManyForOne
 * Implementing the Sink Pattern.
 * Multiple threads publish for one subscriber.
 * (implementation based on the fact that the publisher works in any thread).
 * @note subscriber will not be deleted
 */
template <typename Publication>
class PublishManyForOne
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief PublishManyForOne
	 * Constructor
	 * @param subscription - subscriber, chosen one
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 */
	PublishManyForOne(std::shared_ptr<ISubscription<Publication>> subscription)
		: subscription_{std::move(subscription)}
	{
	}

	/**
	 * @brief PublishManyForOne
	 * Constructor
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param highway_mailbox - where the Runnable is executing (with the captured post and subscription_callback)
	 * @param send_may_fail - whether sending is mandatory: if sending is mandatory, it will wait for free holders in
	 *  high_way_mail_box and thus can block until it gets a free holder to send.
	 *  Objects are placed in Holders and after that you can put them in mail_box_ - this allows
	 *  control memory usage. The number of holders can be increased via the IHighWay->set_capacity(N) method
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 * @note below there is an option to create a subscription without rescheduling sending via highway
	 */
	template <typename R>
	PublishManyForOne(
		R && callback,
		HighWayProxyPtr executor,
		const char * filename,
		unsigned int line,
		bool send_may_fail = true,
		bool for_new_only = false)
		: subscription_{
			for_new_only ? create_subscription_for_new_only<Publication, R>(
				std::move(callback),
				std::move(executor),
				filename,
				line,
				send_may_fail)
						 : create_subscription<Publication, R>(
							 std::move(callback),
							 std::move(executor),
							 filename,
							 line,
							 send_may_fail)}
	{
	}

	template <typename R>
	PublishManyForOne(
		R && callback,
		std::shared_ptr<HighWay> executor,
		const char * filename,
		unsigned int line,
		bool send_may_fail = true,
		bool for_new_only = false)
		: subscription_{
			for_new_only ? create_subscription_for_new_only<Publication, R>(
				std::move(callback),
				std::move(executor),
				filename,
				line,
				send_may_fail)
						 : create_subscription<Publication, R>(
							 std::move(callback),
							 std::move(executor),
							 filename,
							 line,
							 send_may_fail)}
	{
	}

	/**
	 * @brief PublishManyForOne
	 * Constructor
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 */
	template <typename R>
	PublishManyForOne(R && callback, bool for_new_only = false)
		: subscription_{
			for_new_only ? create_subscription_for_new_only<Publication, R>(std::move(callback))
						 : create_subscription<Publication, R>(std::move(callback))}
	{
	}

	void publish(Publication publication) const
	{
		subscription_->send(std::move(publication));
	}

private:
	const std::shared_ptr<ISubscription<Publication>> subscription_;
}; // PublishManyForOne

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_PUBLISH_MANY_FOR_ONE_H
