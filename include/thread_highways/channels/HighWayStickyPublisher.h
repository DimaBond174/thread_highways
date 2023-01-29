/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H
#define THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H

#include <thread_highways/channels/ISubscription.h>
#include <thread_highways/highways/HighWay.h>

#include <optional>

namespace hi {

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

    HighWayStickyPublisher(std::weak_ptr<HighWayStickyPublisher> self_weak, HighWayWeakPtr highway)
        : self_weak_{std::move(self_weak)}
        , highway_{std::move(highway)}
    {
    }

    HighWayStickyPublisher(std::weak_ptr<HighWayStickyPublisher> self_weak, std::shared_ptr<HighWay> highway)
        : self_weak_{std::move(self_weak)}
        , highway_{make_weak_ptr(std::move(highway))}
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
                            subscriptions_.push(new hi::Holder<std::weak_ptr<ISubscription<Publication>>>(std::move(subscription)));
                        }
                    }
                } else {
                    subscriptions_.push(new hi::Holder<std::weak_ptr<ISubscription<Publication>>>(std::move(subscription)));
                }
            }
        }, __FILE__, __LINE__);
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
        last_.emplace(std::move(publication));
    }

private:
   const std::weak_ptr<HighWayStickyPublisher> self_weak_;
   const HighWayWeakPtr highway_;

   SingleThreadStack<Holder<std::weak_ptr<ISubscription<Publication>>>> subscriptions_;
   std::optional<Publication> last_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_HIGHWAY_STICKY_PUBLISHER_H
