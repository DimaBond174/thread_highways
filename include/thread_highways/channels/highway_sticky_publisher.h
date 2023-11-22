/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H
#define THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H

#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/highways/highway.h>

#include <optional>

namespace hi
{

/*
	Подписки и публикации перепланируются на хайвей который под капотом -
	поэтому нет гонок на многопоточке.
	Бонус: доставка сообщений на этом же одном потоке и в некоторых случаях подписчики могут
	использовать то сообщения доставляются без гонок (не перепланировать доставку на поток под капотом подписки).
*/
template <typename Publication>
class HighWayStickyPublisher
{
public:
	typedef Publication PublicationType;

	HighWayStickyPublisher(std::weak_ptr<HighWayStickyPublisher> self_weak, HighWayProxyPtr highway)
		: self_weak_{std::move(self_weak)}
		, highway_{std::move(highway)}
	{
	}

	HighWayStickyPublisher(std::weak_ptr<HighWayStickyPublisher> self_weak, std::shared_ptr<HighWay> highway)
		: self_weak_{std::move(self_weak)}
		, highway_{make_proxy(std::move(highway))}
	{
	}

	void publish(Publication publication)
	{
		highway_->execute(
			[&, publication = std::move(publication)]() mutable
			{
				publish_impl(std::move(publication));
			},
			self_weak_,
			__FILE__,
			__LINE__);
	}

	// Публикации ещё нет, будет готовиться на том же highway с помощью Logic
	template <typename Logic>
	void prepare_and_publish(Logic && logic, const char * filename, unsigned int line)
	{
		highway_->execute(
			[&, logic = std::move(logic)]() mutable
			{
				publish_impl(logic());
			},
			self_weak_,
			filename,
			line);
	}

	// Публикации ещё нет, будет готовиться на том же highway с помощью Logic
	template <typename Logic, typename Protector>
	void prepare_and_publish(Logic && logic, Protector protector, const char * filename, unsigned int line)
	{
		highway_->execute(
			[&, logic = std::move(logic), weak = self_weak_]() mutable
			{
				if (auto alive = weak.lock())
				{
					publish_impl(logic());
				}
			},
			std::move(protector),
			filename,
			line);
	}

	HighWayProxyPtr highway()
	{
		return highway_;
	}

	/**
	 * @brief subscribe
	 * saving a subscription
	 * @param subscription
	 */
	void subscribe(std::weak_ptr<ISubscription<Publication>> subscription)
	{
		highway_->execute(
			[&, subscription = std::move(subscription)]() mutable
			{
				if (last_)
				{
					if (auto subscriber = subscription.lock())
					{
						if (subscriber->send(*last_))
						{
							subscriptions_.push(
								new hi::Holder<std::weak_ptr<ISubscription<Publication>>>(std::move(subscription)));
						}
					}
				}
				else
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
			SubscribeHereImpl(std::weak_ptr<HighWayStickyPublisher> self_weak)
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
			const std::weak_ptr<HighWayStickyPublisher> self_weak_;
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
		last_.emplace(std::move(publication));
	}

private:
	const std::weak_ptr<HighWayStickyPublisher> self_weak_;
	const HighWayProxyPtr highway_;

	SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions_;
	std::optional<Publication> last_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H
