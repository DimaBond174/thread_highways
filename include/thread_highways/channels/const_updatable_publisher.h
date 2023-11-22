/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_CONST_UPDATABLE_PUBLISHER_H
#define THREADS_HIGHWAYS_CHANNELS_CONST_UPDATABLE_PUBLISHER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/tools/stack.h>

namespace hi
{

/**
 * @brief ConstUpdatablePublisher
 * Подписчики не удаляются, но можно добавлять новых.
 * Скорость выше за счёт того что нет преобразований weak_ptr.lock()
 * @note Подписчик будет захвачен как shared_ptr.
 */
template <typename Publication>
class ConstUpdatablePublisher
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief ConstUpdatablePublisher
	 * Constructor
	 * @param self_weak - weak ptr on self
	 * @note usage examples:
	 */
	ConstUpdatablePublisher(std::weak_ptr<ConstUpdatablePublisher<Publication>> self_weak)
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
			SubscribeHereImpl(std::weak_ptr<ConstUpdatablePublisher<Publication>> self_weak)
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
			const std::weak_ptr<ConstUpdatablePublisher<Publication>> self_weak_;
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
		if (auto locked_subscription = subscription.lock())
		{
			subscriptions_safe_stack_.push(
				new Holder<std::shared_ptr<ISubscription<Publication>>>{std::move(locked_subscription)});
		}
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
		for (auto holder = subscriptions_safe_stack_.access_stack(); holder; holder = holder->next_in_stack_)
		{
			if (holder->next_in_stack_)
			{
				holder->t_->send(publication);
			}
			else
			{
				holder->t_->send(std::move(publication));
			}
		}
	}

private:
	const std::weak_ptr<ConstUpdatablePublisher<Publication>> self_weak_;
	mutable ThreadSafeStack<Holder<std::shared_ptr<ISubscription<Publication>>>> subscriptions_safe_stack_;

}; // ConstUpdatablePublisher

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_CONST_UPDATABLE_PUBLISHER_H
