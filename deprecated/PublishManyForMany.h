/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef PUBLISH_MANY_FOR_MANY_H
#define PUBLISH_MANY_FOR_MANY_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <thread>

namespace hi
{

/**
 * @brief PublishManyForMany
 * Implementing the Publisher-Subscribers Pattern.
 * Multiple threads publish to multiple subscribers
 * (implementation based on the fact that the publisher works in any thread).
 * @note subscribers will not be deleted
 * if it is assumed that subscribers can connect and disconnect, then it is better to use:
 * PublishManyForManyCanUnSubscribe
 * or
 * PublishManyForOne + PublishOneForMany (both ends will ForMany)
 */
template <typename Publication>
class PublishManyForMany : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief PublishManyForMany
	 * Constructor
	 * @param self_weak - weak ptr on self
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_many/src/channels_publish_many_for_many.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_many.cpp
	 */
	PublishManyForMany(std::weak_ptr<PublishManyForMany<Publication>> self_weak)
		: self_weak_{std::move(self_weak)}
	{
	}

	/**
	 * @brief subscribe_channel
	 * @return An object that subscribers can use to subscribe to this publisher
	 * @note this object can be safely stored since it only holds the publisher through a weak pointer
	 */
	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishManyForMany<Publication>> self_weak)
				: self_weak_{std::move(self_weak)}
			{
			}
			void subscribe(Subscription<Publication> && subscription) override
			{
				if (auto self = self_weak_.lock())
				{
					self->subscribe(std::move(subscription));
				}
			}
			const std::weak_ptr<PublishManyForMany<Publication>> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

	/**
	 * @brief subscribe
	 * saving a subscription
	 * @param subscription
	 */
	void subscribe(Subscription<Publication> && subscription)
	{
		subscriptions_safe_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
	}

	/**
	 * @brief subscribe
	 * creating and saving a subscription
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
	 * @note below there is an option to create a subscription without rescheduling sending via highway
	 */
	template <typename R, typename P>
	void subscribe(
		R && callback,
		P protector,
		IHighWayMailBoxPtr highway_mailbox,
		const bool send_may_fail = true,
		std::string filename = __FILE__,
		const unsigned int line = __LINE__)
	{
		subscribe(Subscription<Publication>::create(
			std::move(callback),
			std::move(protector),
			std::move(highway_mailbox),
			send_may_fail,
			std::move(filename),
			line));
	} // subscribe

	/**
	 * @brief subscribe
	 * creating and saving a subscription without rescheduling sending via highway
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
	template <typename R, typename P>
	void subscribe(R && callback, P protector, std::string filename = __FILE__, const unsigned int line = __LINE__)
	{
		subscribe(
			Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line));
	} // subscribe

	/**
	 * @brief subscribers_exist
	 * @return true if subscribers exist
	 */
	bool subscribers_exist()
	{
		return !!subscriptions_safe_stack_.access_stack();
	}

public: // IPublisher
	/**
	 * @brief publish
	 * submit publication
	 * @param publication
	 * @note can be called from any thread
	 * @note no control over broken subscriptions
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_many/src/channels_publish_many_for_many.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_many.cpp
	 */
	void publish(Publication publication) const override
	{
		for (Holder<Subscription<Publication>> * holder = subscriptions_safe_stack_.access_stack(); holder;
			 holder = holder->next_in_stack_)
		{
			holder->t_.send(publication);
		}
	}

private:
	const std::weak_ptr<PublishManyForMany<Publication>> self_weak_;
	ThreadSafeStack<Holder<Subscription<Publication>>> subscriptions_safe_stack_;
}; // PublishManyForMany

} // namespace hi

#endif // PUBLISH_MANY_FOR_MANY_H
