/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_HIGHWAYPUBLISHER_H
#define THREADS_HIGHWAYS_CHANNELS_HIGHWAYPUBLISHER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/highways/highway.h>

namespace hi
{

/*
	Подписки и публикации перепланируются на хайвей который под капотом -
	поэтому нет гонок на многопоточке.
	Бонус: подписчики могут не перепланировать доставку на свой поток - использовать прямую доставку
	полагаясь что вызов будет в однопоточке highway
*/
template <typename Publication>
class HighWayPublisher
{
public:
	typedef Publication PublicationType;

	HighWayPublisher(std::weak_ptr<HighWayPublisher> self_weak, HighWayProxyPtr highway)
		: self_weak_{std::move(self_weak)}
		, highway_{std::move(highway)}
	{
	}

	HighWayPublisher(std::weak_ptr<HighWayPublisher> self_weak, std::shared_ptr<HighWay> highway)
		: self_weak_{std::move(self_weak)}
		, highway_{make_proxy(std::move(highway))}
	{
	}

	void publish(Publication publication)
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
	void subscribe(std::weak_ptr<ISubscription<Publication>> subscription)
	{
		highway_->execute(
			[&, subscription = std::move(subscription), weak = self_weak_]() mutable
			{
				if (auto alive = weak.lock())
				{
					subscriptions_.push(
						new hi::Holder<std::weak_ptr<ISubscription<Publication>>>(std::move(subscription)));
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<HighWayPublisher> self_weak)
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
			const std::weak_ptr<HighWayPublisher> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

private:
	void publish_impl(Publication publication)
	{
		SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions;
		while (auto holder = subscriptions_.pop())
		{
			if (auto subscriber = holder->t_.lock())
			{
				const auto res =
					subscriptions_.empty() ? subscriber->send(std::move(publication)) : subscriber->send(publication);
				if (res)
				{
					subscriptions.push(holder);
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
		subscriptions_.swap(subscriptions);
	}

private:
	const std::weak_ptr<HighWayPublisher> self_weak_;
	const HighWayProxyPtr highway_;

	SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_HIGHWAYPUBLISHER_H
