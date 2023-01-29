/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_CONNECTIONS_H
#define THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_CONNECTIONS_H

#include <thread_highways/channels/ISubscription.h>
#include <thread_highways/highways/HighWay.h>

#include <optional>

namespace hi {

/**
 * @brief HighWayStickyPublisherWithConnectionsNotifier
 * Implementing the Publisher-Subscribers Pattern.
 * Tracks the connection of the first subscriber and the disconnection
 * of the last subscriber - this is convenient for the publisher service to work
 * only at the moment when the service is needed by someone.
 * HighWay helps to avoid thread races.

    Подписки и публикации перепланируются на хайвей который под капотом -
    поэтому нет гонок на многопоточке.
    Бонус: доставка сообщений на этом же одном потоке и в некоторых случаях подписчики могут
    использовать то сообщения доставляются без гонок (не перепланировать доставку на поток под капотом подписки).

 * @note broken subscriptions will be deleted, subscribers can connect and disconnect
 */
template <typename Publication>
class HighWayStickyPublisherWithConnectionsNotifier
{
public:
    typedef Publication PublicationType;

    template <typename OnFirstConnected, typename OnLastDisconnected>
    HighWayStickyPublisherWithConnectionsNotifier(
            std::weak_ptr<HighWayStickyPublisherWithConnectionsNotifier> self_weak,
            OnFirstConnected&& on_first_connected,
            OnLastDisconnected&& on_last_disconnected,
            HighWayWeakPtr highway
    )
        : self_weak_{std::move(self_weak)}
        , highway_{std::move(highway)}
    {
        struct OnFirstLastNotifier : public IOnFirstLastNotifier
        {
                OnFirstLastNotifier(OnFirstConnected && on_first_connected, OnLastDisconnected && on_last_disconnected)
                        : on_first_connected_{std::move(on_first_connected)}
                        , on_last_disconnected_{std::move(on_last_disconnected)}
                {
                }

                void on_first_connected() const noexcept override
                {
                        on_first_connected_();
                }

                void on_last_disconnected() const noexcept override
                {
                        on_last_disconnected_();
                }

                OnFirstConnected on_first_connected_;
                OnLastDisconnected on_last_disconnected_;
        };
        notifier_ = std::make_unique<OnFirstLastNotifier>(std::move(on_first_connected), std::move(on_last_disconnected));
    }

    template <typename OnFirstConnected, typename OnLastDisconnected>
    HighWayStickyPublisherWithConnectionsNotifier(
            std::weak_ptr<HighWayStickyPublisherWithConnectionsNotifier> self_weak,
            OnFirstConnected&& on_first_connected,
            OnLastDisconnected&& on_last_disconnected,
            std::shared_ptr<HighWay> highway)
        : HighWayStickyPublisherWithConnectionsNotifier{std::move(self_weak),
                                                        std::move(on_first_connected),
                                                        std::move(on_last_disconnected),
                                                        make_weak_ptr(std::move(highway))}
    {
    }

    void publish(Publication publication)
    {
        highway_->execute([&, publication = std::move(publication), weak = self_weak_]() mutable
        {
            if (auto alive = weak.lock())
            {
                publish_impl(std::move(publication));
            }
        }, __FILE__, __LINE__);
    }

    /**
     * @brief subscribe
     * saving a subscription
     * @param subscription
     */
    void subscribe(std::weak_ptr<ISubscription<Publication>> subscription)
    {
        highway_->execute([&, subscription = std::move(subscription), weak = self_weak_]() mutable
        {
            if (auto alive = weak.lock())
            {
                if (last_)
                {
                    if (auto subscriber = subscription.lock())
                    {
                        if (subscriber->send(*last_))
                        {
                            subscribe_impl(std::move(subscription));
                        }
                    }
                } else {
                    subscribe_impl(std::move(subscription));
                }
            }
        }, __FILE__, __LINE__);
    }

    ISubscribeHerePtr<Publication> subscribe_channel()
    {
        struct SubscribeHereImpl : public ISubscribeHere<Publication>
        {
            SubscribeHereImpl(std::weak_ptr<HighWayStickyPublisherWithConnectionsNotifier> self_weak)
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
            const std::weak_ptr<HighWayStickyPublisherWithConnectionsNotifier> self_weak_;
        };
        return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
    }

private:
    void subscribe_impl(std::weak_ptr<ISubscription<Publication>> subscription)
    {
        if (subscriptions_.empty())
        {
            notifier_->on_first_connected();
        }
        subscriptions_.push(new hi::Holder<std::weak_ptr<ISubscription<Publication>>>(std::move(subscription)));
    }

    void publish_impl(Publication publication)
    {
        SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions;
        while(auto holder = subscriptions_.pop())
        {
            if (auto subscriber = holder->t_.lock())
            {
                if (subscriber->send(publication))
                {
                    subscriptions.push(holder);
                } else {
                    delete holder;
                }
            } else {
                delete holder;
            }
        }
        subscriptions_.swap(subscriptions);
        if (subscriptions_.empty())
        {
            notifier_->on_last_disconnected();
        }
        last_.emplace(std::move(publication));
    }

private:
        struct IOnFirstLastNotifier
        {
                virtual ~IOnFirstLastNotifier() = default;
                virtual void on_first_connected() const noexcept = 0;
                virtual void on_last_disconnected() const noexcept = 0;
        };

private:
   const std::weak_ptr<HighWayStickyPublisherWithConnectionsNotifier> self_weak_;
   std::unique_ptr<IOnFirstLastNotifier> notifier_;
   const HighWayWeakPtr highway_;

   SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions_;
   std::optional<Publication> last_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_CONNECTIONS_H
