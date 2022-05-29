/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef PUBLISH_ONE_FOR_MANY_H
#define PUBLISH_ONE_FOR_MANY_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace hi
{

/**
 * @brief PublishOneForMany
 * Implementing the Publisher-Subscribers Pattern.
 * Single threads publish to multiple subscribers
 * (implementation based on the fact that the publisher works in single thread).
 * @note broken subscriptions will be deleted, subscribers can connect and disconnect
 */
template <typename Publication>
class PublishOneForMany : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief PublishOneForMany
	 * Constructor
	 * @param self_weak - weak ptr on self
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_one_for_many/src/channels_publish_one_for_many.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_one_for_many.cpp
	 */
	PublishOneForMany(std::weak_ptr<PublishOneForMany<Publication>> self_weak)
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
			SubscribeHereImpl(std::weak_ptr<PublishOneForMany<Publication>> self_weak)
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
			const std::weak_ptr<PublishOneForMany<Publication>> self_weak_;
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

public: // IPublisher
	/**
	 * @brief publish
	 * submit publication
	 * @param publication
	 * @note can be called from single thread
	 * @note broken subscriptions will be deleted
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_one_for_many/src/channels_publish_one_for_many.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_one_for_many.cpp
	 */
	void publish(Publication publication) const override
	{
#ifndef NDEBUG
		check_local_thread();
#endif
		subscriptions_safe_stack_.move_to(local_work_stack_);
		SingleThreadStack<Holder<Subscription<Publication>>> local_tmp_stack;
		while (Holder<Subscription<Publication>> * holder = local_work_stack_.pop())
		{
			if (holder->t_.send(publication))
			{
				local_tmp_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		}
		local_tmp_stack.swap(local_work_stack_);
	}

private:
	void check_local_thread() const
	{
		if (!local_thread_id_)
		{
			local_thread_id_.emplace(std::this_thread::get_id());
		}
		if (*local_thread_id_ != std::this_thread::get_id())
		{
			throw std::logic_error("PublishOneForMany: must use single thread only for this operation");
		}
	}

private:
	const std::weak_ptr<PublishOneForMany<Publication>> self_weak_;
	mutable ThreadSafeStack<Holder<Subscription<Publication>>> subscriptions_safe_stack_;
	mutable SingleThreadStack<Holder<Subscription<Publication>>> local_work_stack_;
	mutable std::optional<std::thread::id> local_thread_id_;
}; // PublishOneForMany

} // namespace hi

#endif // PUBLISH_ONE_FOR_MANY_H
