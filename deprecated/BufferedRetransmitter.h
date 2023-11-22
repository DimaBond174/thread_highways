/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef PUBLISHFROMBUFFEREDRETRANSMITTER_H
#define PUBLISHFROMBUFFEREDRETRANSMITTER_H

#include <thread_highways/channels/IPublishSubscribe.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>

namespace hi
{

/**
 * PublicationHolder
 *
 * Wrapper for storing a publication under a mutex or in an atomic
 */
template <typename Publication, bool>
struct PublicationHolder;

template <typename Publication>
struct PublicationHolder<Publication, true>
{
	void set_value(Publication value)
	{
		value_.store(value, std::memory_order_release);
	}

	Publication get_value()
	{
		return value_.load(std::memory_order_acquire);
	}

	std::atomic<Publication> value_{};
};

template <typename Publication>
struct PublicationHolder<Publication, false>
{
	void set_value(Publication value)
	{
		std::lock_guard lg{mutex_};
		value_ = std::move(value);
	}

	Publication get_value()
	{
		std::lock_guard lg{mutex_};
		return value_;
	}

	std::mutex mutex_;
	Publication value_{};
};

/**
 * BufferedRetransmitter
 *
 * Subscribes to the Publisher and stores the latest message.
 * Relays the message to its subscribers according to the settings:
 * resend_to_just_connected = true, if you need to send a latest message to new subscribers
 * send_new_only = true, if it is necessary to filter messages
 *	and forward only if the data has changed since the last message
 * @note if necessary to keep the original message null, then use
 *	Publication as std::optional<Publication> or std::shared_ptr<Publication>
 */
template <typename Publication, bool resend_to_just_connected = true, bool send_new_only = true>
class BufferedRetransmitter
{
public:
	typedef Publication PublicationType;

	BufferedRetransmitter(
		std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak,
		ISubscribeHerePtr<Publication> subscribe_channel,
		Publication default_publication = {})
		: self_weak_{std::move(self_weak)}
		, subscribe_channel_{std::move(subscribe_channel)}
		,
	{

		holder_.set_value(std::move(default_publication));
		struct SubscriptionHolderImpl : public Subscription<Publication>::SubscriptionHolder
		{
			SubscriptionHolderImpl(
				std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak)
				: self_weak_{std::move(self_weak)}
			{
			}

			bool operator()(Publication publication) override
			{
				if (auto self = self_weak_.lock())
				{
					self->set_value(std::move(publication));
					return true;
				}
				return false;
			}

			std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak_;
		}; // SubscriptionHolderImpl
		subscribe_channel_->subscribe(Subscription<Publication>{new SubscriptionHolderImpl{self_weak_}});
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		if constexpr (send_new_only)
		{
			if constexpr (resend_to_just_connected)
			{
				return std::make_shared<SubscribeHereResendNewOnly>(self_weak_, subscribe_channel_);
			}
			else
			{
				return std::make_shared<SubscribeHereNewOnly>(self_weak_, subscribe_channel_);
			} // if !resend_to_just_connected
		}
		else
		{
			if constexpr (resend_to_just_connected)
			{
				return std::make_shared<SubscribeHereResend>(self_weak_, subscribe_channel_);
			}
			else
			{
				return subscribe_channel_;
			}
		} // if !send_new_only
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
		subscribe_channel()->subscribe(Subscription<Publication>::create(
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
		subscribe_channel()->subscribe(
			Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line));
	} // subscribe

	void set_value(Publication value)
	{
		holder_.set_value(std::move(value));
	}

	Publication get_value()
	{
		return holder_.get_value();
	}

private:
	struct SubscribeHereResendNewOnly : public ISubscribeHere<Publication>
	{
		SubscribeHereResendNewOnly(
			std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak,
			ISubscribeHerePtr<Publication> subscribe_channel)
			: self_{std::move(self_weak)}
			, channel_{std::move(subscribe_channel)}
		{
		}

		void subscribe(Subscription<Publication> && subscription) override
		{
			if (auto self = self_.lock())
			{
				auto value = self->get_value();
				struct SubscriptionHolderImpl : public Subscription<Publication>::SubscriptionHolder
				{
					SubscriptionHolderImpl(Subscription<Publication> && subscription, Publication last_value)
						: subscription_{std::move(subscription)}
						, last_value_{std::move(last_value)}
					{
					}

					bool operator()(Publication publication) override
					{
						if (last_value_ != publication)
						{
							last_value_ = publication;
							return subscription_.send(std::move(publication));
						}
						return true;
					}

					Subscription<Publication> subscription_;
					Publication last_value_;
				}; // SubscriptionHolderImpl

				subscription.send(value);
				channel_->subscribe(
					Subscription<Publication>{new SubscriptionHolderImpl{std::move(subscription), std::move(value)}});
			} // if
		} // subscribe
		const std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_;
		const ISubscribeHerePtr<Publication> channel_;
	}; // SubscribeHereResendNewOnly

	struct SubscribeHereNewOnly : public ISubscribeHere<Publication>
	{
		SubscribeHereNewOnly(
			std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak,
			ISubscribeHerePtr<Publication> subscribe_channel)
			: self_{std::move(self_weak)}
			, channel_{std::move(subscribe_channel)}
		{
		}

		void subscribe(Subscription<Publication> && subscription) override
		{
			if (auto self = self_.lock())
			{
				struct SubscriptionHolderImpl : public Subscription<Publication>::SubscriptionHolder
				{
					SubscriptionHolderImpl(Subscription<Publication> && subscription, Publication last_value)
						: subscription_{std::move(subscription)}
						, last_value_{std::move(last_value)}
					{
					}

					bool operator()(Publication publication) override
					{
						if (last_value_ != publication)
						{
							last_value_ = publication;
							return subscription_.send(std::move(publication));
						}
						return true;
					}

					Subscription<Publication> subscription_;
					Publication last_value_;
				}; // SubscriptionHolderImpl

				channel_->subscribe(
					Subscription<Publication>{new SubscriptionHolderImpl{std::move(subscription), self->get_value()}});
			} // if
		} // subscribe
		const std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_;
		const ISubscribeHerePtr<Publication> channel_;
	}; // SubscribeHereNewOnly

	struct SubscribeHereResend : public ISubscribeHere<Publication>
	{
		SubscribeHereResend(
			std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak,
			ISubscribeHerePtr<Publication> subscribe_channel)
			: self_{std::move(self_weak)}
			, channel_{std::move(subscribe_channel)}
		{
		}

		void subscribe(Subscription<Publication> && subscription) override
		{
			if (auto self = self_.lock())
			{
				struct SubscriptionHolderImpl : public Subscription<Publication>::SubscriptionHolder
				{
					SubscriptionHolderImpl(Subscription<Publication> && subscription)
						: subscription_{std::move(subscription)}
					{
					}

					bool operator()(Publication publication) override
					{
						return subscription_.send(std::move(publication));
					}

					Subscription<Publication> subscription_;
				}; // SubscriptionHolderImpl

				subscription.send(self->get_value());
				channel_->subscribe(Subscription<Publication>{new SubscriptionHolderImpl{std::move(subscription)}});

			} // if
		} // subscribe
		const std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_;
		const ISubscribeHerePtr<Publication> channel_;
	}; // SubscribeHereResend

private:
	const std::weak_ptr<BufferedRetransmitter<Publication, resend_to_just_connected, send_new_only>> self_weak_;
	const ISubscribeHerePtr<Publication> subscribe_channel_;
	PublicationHolder<Publication, std::is_trivially_copyable<Publication>::value> holder_;
};

} // namespace hi

#endif // PUBLISHFROMBUFFEREDRETRANSMITTER_H
