/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_PUBLISH_ONE_FOR_MANY_H
#define THREADS_HIGHWAYS_CHANNELS_PUBLISH_ONE_FOR_MANY_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/tools/stack.h>

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
class PublishOneForMany
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
			void subscribe(std::weak_ptr<ISubscription<Publication>> subscription) override
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
	void subscribe(std::weak_ptr<ISubscription<Publication>> subscription)
	{
		subscriptions_safe_stack_.push(new Holder<std::weak_ptr<ISubscription<Publication>>>{std::move(subscription)});
	}

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
	void publish(Publication publication) const
	{
		subscriptions_safe_stack_.move_to(local_work_stack_);
		SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> local_tmp_stack;
		while (auto holder = local_work_stack_.pop())
		{
			if (auto subscriber = holder->t_.lock())
			{
				const auto res = local_work_stack_.empty() ? subscriber->send(std::move(publication))
														   : subscriber->send(publication);
				if (res)
				{
					local_tmp_stack.push(holder);
				}
				else
				{
					delete holder;
				}
			}
			else
			{
				delete holder;
			}
		}
		local_tmp_stack.swap(local_work_stack_);
	}

private:
	const std::weak_ptr<PublishOneForMany<Publication>> self_weak_;
	mutable ThreadSafeStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions_safe_stack_;

private: // thread_local
	mutable SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> local_work_stack_;

}; // PublishOneForMany

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_PUBLISH_ONE_FOR_MANY_H
