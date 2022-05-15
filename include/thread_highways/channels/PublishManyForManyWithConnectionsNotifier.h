#ifndef PUBLISH_MANY_FOR_MANY_CAN_WithConnectionsNotifierH
#define PUBLISH_MANY_FOR_MANY_CAN_WithConnectionsNotifierH

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace hi
{

template <typename Publication>
class PublishManyForManyWithConnectionsNotifier : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	template <typename OnFirstConnected, typename OnLastDisconnected>
	PublishManyForManyWithConnectionsNotifier(
		std::weak_ptr<PublishManyForManyWithConnectionsNotifier<Publication>> self_weak,
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
	PublishManyForManyWithConnectionsNotifier(
		std::weak_ptr<PublishManyForManyWithConnectionsNotifier<Publication>> self_weak)
		: PublishManyForManyWithConnectionsNotifier{std::move(self_weak), [] {}, [] {}}
	{
	}

	~PublishManyForManyWithConnectionsNotifier()
	{
		delete notifier_;
	}
	PublishManyForManyWithConnectionsNotifier(const PublishManyForManyWithConnectionsNotifier &) = delete;
	PublishManyForManyWithConnectionsNotifier & operator=(const PublishManyForManyWithConnectionsNotifier &) = delete;
	PublishManyForManyWithConnectionsNotifier(PublishManyForManyWithConnectionsNotifier && rhs)
		: notifier_{rhs.notifier_}
	{
		subscriptions_stack_.swap(rhs.subscriptions_stack_);
		rhs.notifier = nullptr;
	}

	PublishManyForManyWithConnectionsNotifier & operator=(PublishManyForManyWithConnectionsNotifier && rhs)
	{
		if (this == &rhs)
			return *this;

		subscriptions_stack_.swap(rhs.subscriptions_stack_);
		delete notifier_;
		notifier_ = rhs.notifier;
		rhs.notifier = nullptr;
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishManyForManyWithConnectionsNotifier<Publication>> self_weak)
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
			const std::weak_ptr<PublishManyForManyWithConnectionsNotifier<Publication>> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

	void subscribe(Subscription<Publication> && subscription)
	{
		std::lock_guard lg{mutex_};
		const bool first_subscription = !subscriptions_stack_.get_first();
		subscriptions_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
		if (first_subscription)
		{
			notifier_->on_first_connected();
		}
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

	bool subscribers_exist()
	{
		std::lock_guard lg{mutex_};
		return !!subscriptions_stack_.get_first();
	}

public: // IPublisher
	void publish(Publication publication) const override
	{
		std::lock_guard lg{mutex_};
		SingleThreadStack<Holder<Subscription<Publication>>> local_tmp_stack;
		for (Holder<Subscription<Publication>> * holder = subscriptions_stack_.pop(); holder;
			 holder = subscriptions_stack_.pop())
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
		if (!local_tmp_stack.get_first())
		{
			notifier_->on_last_disconnected();
			return;
		}
		local_tmp_stack.swap(subscriptions_stack_);
	}

private:
	struct IOnFirstLastNotifier
	{
		virtual ~IOnFirstLastNotifier() = default;
		virtual void on_first_connected() const noexcept = 0;
		virtual void on_last_disconnected() const noexcept = 0;
	};

private:
	const std::weak_ptr<PublishManyForManyWithConnectionsNotifier<Publication>> self_weak_;
	mutable std::recursive_mutex mutex_;
	IOnFirstLastNotifier * notifier_{nullptr};
	mutable SingleThreadStack<Holder<Subscription<Publication>>> subscriptions_stack_;
}; // PublishManyForManyWithConnectionsNotifier

} // namespace hi

#endif // PUBLISH_MANY_FOR_MANY_CAN_WithConnectionsNotifierH
