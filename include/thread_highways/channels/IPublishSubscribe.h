#ifndef I_PUBLISHER_H
#define I_PUBLISHER_H

#include <thread_highways/highways/IHighWay.h>

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

namespace hi
{

template <typename Publication>
class SubscriptionCallback
{
public:
	template <typename R, typename P>
	static std::shared_ptr<SubscriptionCallback> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line)
	{
		struct SubscriptionCallbackProtectedHolderImpl : public SubscriptionCallbackHolder
		{
			SubscriptionCallbackProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(Publication publication) override
			{
				safe_invoke_void(callback_, protector_, std::move(publication));
			}

			[[nodiscard]] bool alive() override
			{
				if (auto lock = protector_.lock())
				{
					return true;
				}
				return false;
			}

			R callback_;
			P protector_;
		};
		return std::make_shared<SubscriptionCallback>(
			new SubscriptionCallbackProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line);
	}

	~SubscriptionCallback()
	{
		delete subscription_callback_holder_;
	}
	SubscriptionCallback(const SubscriptionCallback & rhs) = delete;
	SubscriptionCallback & operator=(const SubscriptionCallback & rhs) = delete;
	SubscriptionCallback(SubscriptionCallback && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	SubscriptionCallback & operator=(SubscriptionCallback && rhs)
	{
		if (this == &rhs)
			return *this;
		filename_ = std::move(rhs.callback_);
		line_ = rhs.line_;
		subscription_callback_holder_ = rhs.subscription_callback_holder_;
		rhs.subscription_callback_holder_ = nullptr;
		return *this;
	}

	void send(Publication publication) const
	{
		(*subscription_callback_holder_)(std::move(publication));
	}

	bool alive()
	{
		return subscription_callback_holder_->alive();
	}

	std::string get_code_filename()
	{
		return filename_;
	}

	unsigned int get_code_line()
	{
		return line_;
	}

	struct SubscriptionCallbackHolder
	{
		virtual ~SubscriptionCallbackHolder() = default;
		virtual void operator()(Publication publication) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	SubscriptionCallback(SubscriptionCallbackHolder * subscription_holder, std::string filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, subscription_callback_holder_{subscription_holder}
	{
	}

private:
	std::string filename_;
	unsigned int line_;
	SubscriptionCallbackHolder * subscription_callback_holder_;
}; // SubscriptionCallback

template <typename Publication>
using SubscriptionCallbackPtr = std::shared_ptr<SubscriptionCallback<Publication>>;

template <typename Publication, bool send_may_fail = true>
class Subscription
{
public:
	/*
		subscription_callback нужен в shared_ptr т.к. постоянно копируется в Runnable что постится на IHighWayMailBoxPtr
	*/
	static Subscription create(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr high_way_mail_box)
	{
		assert(subscription_callback);
		struct SubscriptionProtectedHolderImpl : public SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				SubscriptionCallbackPtr<Publication> subscription_callback,
				IHighWayMailBoxPtr high_way_mail_box)
				: subscription_callback_{std::move(subscription_callback)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
			{
			}

			bool operator()(Publication publication) override
			{
				if (!subscription_callback_->alive())
				{
					return false;
				}

				auto message = hi::Runnable::create(
					[subscription_callback = subscription_callback_, publication = std::move(publication)](
						const std::atomic<std::uint32_t> &,
						const std::uint32_t) mutable
					{
						subscription_callback->send(std::move(publication));
					},
					subscription_callback_->get_code_filename(),
					subscription_callback_->get_code_line());

				if constexpr (send_may_fail)
				{
					return high_way_mail_box_->send_may_fail(std::move(message));
				}
				else
				{
					return high_way_mail_box_->send_may_blocked(std::move(message));
				}
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription{
			new SubscriptionProtectedHolderImpl{std::move(subscription_callback), std::move(high_way_mail_box)}};
	}

	~Subscription()
	{
		delete subscription_holder_;
	}
	Subscription(const Subscription & rhs) = delete;
	Subscription & operator=(const Subscription & rhs) = delete;
	Subscription(Subscription && rhs)
		: subscription_holder_{rhs.subscription_holder_}
	{
		assert(subscription_holder_);
		rhs.subscription_holder_ = nullptr;
	}
	Subscription & operator=(Subscription && rhs)
	{
		if (this == &rhs)
			return *this;
		subscription_holder_ = rhs.subscription_holder_;
		assert(subscription_holder_);
		rhs.subscription_holder_ = nullptr;
		return *this;
	}

	bool send(Publication publication) const
	{
		return (*subscription_holder_)(std::move(publication));
	}

	struct SubscriptionHolder
	{
		virtual ~SubscriptionHolder() = default;
		virtual bool operator()(Publication publication) = 0;
	};

	Subscription(SubscriptionHolder * subscription_holder)
		: subscription_holder_{subscription_holder}
	{
		assert(subscription_holder_);
	}

private:
	SubscriptionHolder * subscription_holder_;
}; // Subscription

/*
 Интерфейс в котором будущие подписчики смогут подписаться
*/
template <typename Publication>
struct ISubscribeHere
{
	virtual ~ISubscribeHere() = default;

	// Для UnSubscribe надо чтобы protector в SubscriptionCallback перестал lock() отрабатывать
	virtual void subscribe(Subscription<Publication> && subscription) = 0;
};

template <typename Publication>
using ISubscribeHerePtr = std::shared_ptr<ISubscribeHere<Publication>>;

/*
 Интерфейс в котором публиковать
*/
template <typename Publication>
struct IPublisher
{
	virtual ~IPublisher() = default;

	virtual void publish(Publication publication) const = 0;
};

template <typename Publication>
using IPublisherPtr = std::shared_ptr<IPublisher<Publication>>;

} // namespace hi

#endif // I_PUBLISHER_H
