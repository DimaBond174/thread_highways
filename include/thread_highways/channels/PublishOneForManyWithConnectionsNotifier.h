/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef PUBLISH_ONE_FOR_MANY_WithConnectionsNotifierH
#define PUBLISH_ONE_FOR_MANY_WithConnectionsNotifierH

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace hi
{

/*!
 * Один поток публикует для нескольких подписчиков
 * (реализация исходя из того что паблишер работает в 1м потоке)
 *
 *
 */
template <typename Publication>
class PublishOneForManyWithConnectionsNotifier : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	template <typename OnFirstConnected, typename OnLastDisconnected>
	PublishOneForManyWithConnectionsNotifier(
		std::weak_ptr<PublishOneForManyWithConnectionsNotifier<Publication>> self_weak,
		OnFirstConnected && on_first_connected,
		OnLastDisconnected && on_last_disconnected)
		: self_weak_{std::move(self_weak)}
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
		notifier_ = new OnFirstLastNotifier(std::move(on_first_connected), std::move(on_last_disconnected));
	}

	// For unit tests only
	PublishOneForManyWithConnectionsNotifier(
		std::weak_ptr<PublishOneForManyWithConnectionsNotifier<Publication>> self_weak)
		: PublishOneForManyWithConnectionsNotifier{std::move(self_weak), [] {}, [] {}}
	{
	}

	~PublishOneForManyWithConnectionsNotifier()
	{
		delete notifier_;
	}
	PublishOneForManyWithConnectionsNotifier(const PublishOneForManyWithConnectionsNotifier &) = delete;
	PublishOneForManyWithConnectionsNotifier & operator=(const PublishOneForManyWithConnectionsNotifier &) = delete;
	PublishOneForManyWithConnectionsNotifier(PublishOneForManyWithConnectionsNotifier && rhs)
		: notifier_{rhs.notifier_}
	{
		subscriptions_safe_stack_.swap(rhs.subscriptions_safe_stack_.get_stack());
		work_safe_stack_.swap(rhs.work_safe_stack_.get_stack());
		rhs.notifier = nullptr;
	}

	PublishOneForManyWithConnectionsNotifier & operator=(PublishOneForManyWithConnectionsNotifier && rhs)
	{
		if (this == &rhs)
			return *this;
		subscriptions_safe_stack_.swap(rhs.subscriptions_safe_stack_.get_stack());
		work_safe_stack_.swap(rhs.work_safe_stack_.get_stack());
		delete notifier_;
		notifier_ = rhs.notifier;
		rhs.notifier = nullptr;
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishOneForManyWithConnectionsNotifier<Publication>> self_weak)
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
			const std::weak_ptr<PublishOneForManyWithConnectionsNotifier<Publication>> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

	void subscribe(Subscription<Publication> && subscription)
	{
		if (!subscriptions_safe_stack_.access_stack() && !work_safe_stack_.access_stack())
		{
			std::lock_guard lg{notifier_mutex_};
			if (!subscriptions_safe_stack_.access_stack() && !work_safe_stack_.access_stack())
			{
				subscriptions_safe_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
				notifier_->on_first_connected();
				return;
			}
		}
		subscriptions_safe_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
	}

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

	template <typename R, typename P>
	void subscribe(R && callback, P protector, std::string filename = __FILE__, const unsigned int line = __LINE__)
	{
		subscribe(
			Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line));
	} // subscribe

public: // IPublisher
	void publish(Publication publication) const override
	{
#ifndef NDEBUG
		check_local_thread();
#endif
		// Тут могу использовать ThreadSafeStack::pop() так как уничтожить холдер может только этот же поток
		for (auto holder = subscriptions_safe_stack_.pop(); holder; holder = subscriptions_safe_stack_.pop())
		{
			if (holder->t_.send(publication))
			{
				work_safe_stack_.push(holder);
			}
			else
			{
				delete holder;
			}
		}

		if (!work_safe_stack_.access_stack())
		{
			std::lock_guard lg{notifier_mutex_};
			if (!subscriptions_safe_stack_.access_stack())
			{
				notifier_->on_last_disconnected();
				return;
			}
		}
		work_safe_stack_.move_to(subscriptions_safe_stack_);
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
			throw std::logic_error(
				"PublishOneForManyWithConnectionsNotifier: must use single thread only for this operation");
		}
	}

private:
	struct IOnFirstLastNotifier
	{
		virtual ~IOnFirstLastNotifier() = default;
		virtual void on_first_connected() const noexcept = 0;
		virtual void on_last_disconnected() const noexcept = 0;
	};

private:
	const std::weak_ptr<PublishOneForManyWithConnectionsNotifier<Publication>> self_weak_;
	mutable std::recursive_mutex notifier_mutex_;
	IOnFirstLastNotifier * notifier_{nullptr};
	mutable ThreadSafeStack<Holder<Subscription<Publication>>> subscriptions_safe_stack_;
	mutable ThreadSafeStack<Holder<Subscription<Publication>>> work_safe_stack_;
	mutable std::optional<std::thread::id> local_thread_id_;
}; // PublishOneForManyWithConnectionsNotifier

} // namespace hi

#endif // PUBLISH_ONE_FOR_MANY_WithConnectionsNotifierH
