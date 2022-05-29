/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

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

/**
 * SubscriptionCallback
 *
 * Subscriber callback wrapper with subscription lifetime control.
 * @note You can, by analogy with the factory create(), make your own implementations of SubscriptionCallback.
 */
template <typename Publication>
class SubscriptionCallback
{
public:
	/**
	 * @brief The LaunchParameters struct
	 * The structure groups all incoming parameters for convenience.
	 * This is convenient because with an increase in the number of
	 * incoming parameters, you will not have to make changes to previously developed callbacks.
	 */
	struct LaunchParameters
	{
		// incoming data (publication)
		Publication publication_;
		// identifier with which this highway works now
		const std::reference_wrapper<const std::atomic<std::uint32_t>> global_run_id_;
		// your_run_id - identifier with which this highway was running when this task started
		const std::uint32_t your_run_id_;
	};

	/**
	 * Factory for creating of default SubscriptionCallback
	 * @param callback - code to execute (must implements operator())
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 */
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

			bool has_run_id_control() const noexcept override
			{
				if constexpr (
					std::is_invocable_v<R, Publication, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					return true;
				}
				else if constexpr (std::is_invocable_v<R, LaunchParameters>)
				{
					return true;
				}
				return false;
			}

			bool send([[maybe_unused]] Publication publication) override
			{
				if constexpr (std::is_invocable_v<R, Publication>)
				{
					return safe_invoke_protection_result(callback_, protector_, std::move(publication));
				}
				else if constexpr (std::is_invocable_v<R>)
				{
					return safe_invoke_protection_result(callback_, protector_);
				}
				// Could not find a compatible signature
				// Note: Direct send does not use callbacks with highway control (global_run_id ..)
				assert(false);
				return false;
			}

			bool send(
				[[maybe_unused]] Publication publication,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<R, LaunchParameters>)
				{
					return safe_invoke_protection_result(
						callback_,
						protector_,
						LaunchParameters{std::move(publication), global_run_id, your_run_id});
				}
				else if constexpr (
					std::is_invocable_v<R, Publication, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					return safe_invoke_protection_result(
						callback_,
						protector_,
						std::move(publication),
						global_run_id,
						your_run_id);
				}
				else
				{
					return send(std::move(publication));
				}
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

	bool has_run_id_control() const noexcept
	{
		return subscription_callback_holder_->has_run_id_control();
	}

	bool send(Publication publication) const
	{
		return subscription_callback_holder_->send(std::move(publication));
	}

	bool send(
		Publication publication,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		return subscription_callback_holder_->send(std::move(publication), global_run_id, your_run_id);
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
		virtual bool has_run_id_control() const noexcept = 0;
		virtual bool send(Publication publication) = 0;
		virtual bool send(
			Publication publication,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id) = 0;
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

/**
 * Subscription
 *
 * The generic object that the publisher works with.
 */
template <typename Publication>
class Subscription
{
public:
	/**
	 * @brief create
	 * Subscription Factory, with shipping via highway
	 * @param subscription_callback - where to send the publication,
	 *  it is needed in shared_ptr because it is constantly copied to Runnable
	 * @param highway_mailbox - where the Runnable is executing (with the captured post and subscription_callback)
	 * @param send_may_fail - whether sending is mandatory: if sending is mandatory, it will wait for free holders in
	 *  high_way_mail_box and thus can block until it gets a free holder to send.
	 *  Objects are placed in Holders and after that you can put them in mail_box_ - this allows
	 *  control memory usage. The number of holders can be increased via the IHighWay->set_capacity(N) method
	 * @return Subscription
	 * @note The factory below, through create_direct_send, creates a subscription through
	 *  which messages are sent without changing the thread highway and therefore without
	 *  holders and instantly
	 */
	static Subscription create(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mailbox,
		bool send_may_fail = true)
	{
		assert(subscription_callback);
		return send_may_fail ? create_send_may_fail(std::move(subscription_callback), std::move(highway_mailbox))
							 : create_send_may_blocked(std::move(subscription_callback), std::move(highway_mailbox));
	}

	/**
	 * @brief create
	 * Subscription Factory, with instantly shipping
	 * @param subscription_callback - where to send the publication
	 * @return Subscription
	 * @note for universality, the same SubscriptionCallbackPtr is used as for the factory with highways
	 */
	static Subscription create(SubscriptionCallbackPtr<Publication> subscription_callback)
	{
		return create_direct_send(std::move(subscription_callback));
	}

	/**
	 * @brief create
	 * Subscription Factory, with shipping via highway
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param highway_mailbox - where the Runnable is executing (with the captured post and subscription_callback)
	 * @param send_may_fail - whether sending is mandatory: if sending is mandatory, it will wait for free holders in
	 *  high_way_mail_box and thus can block until it gets a free holder to send.
	 *  Objects are placed in Holders and after that you can put them in mail_box_ - this allows
	 *  control memory usage. The number of holders can be increased via the IHighWay->set_capacity(N) method
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @return Subscription
	 * @note The factory below, through create_direct_send, creates a subscription through
	 *  which messages are sent without changing the thread highway and therefore without
	 *  holders and instantly
	 */
	template <typename R, typename P>
	static Subscription create(
		R && callback,
		P protector,
		IHighWayMailBoxPtr highway_mailbox,
		bool send_may_fail = true,
		std::string filename = __FILE__,
		const unsigned int line = __LINE__)
	{
		auto subscription_callback = SubscriptionCallback<Publication>::template create<R, P>(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line);

		return create(std::move(subscription_callback), std::move(highway_mailbox), send_may_fail);
	} // create

	/**
	 * @brief create
	 * Subscription Factory, with instantly shipping
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @return Subscription
	 */
	template <typename R, typename P>
	static Subscription create(
		R && callback,
		P protector,
		std::string filename = __FILE__,
		const unsigned int line = __LINE__)
	{
		auto subscription_callback = SubscriptionCallback<Publication>::template create<R, P>(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line);

		return create(std::move(subscription_callback));
	} // create

	static Subscription create_send_may_fail(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
		if (subscription_callback->has_run_id_control())
		{
			return create_send_may_fail_with_run_id_control(
				std::move(subscription_callback),
				std::move(highway_mail_box));
		}
		else
		{
			return create_send_may_fail_no_run_id_control(
				std::move(subscription_callback),
				std::move(highway_mail_box));
		}
	}

	static Subscription create_send_may_fail_with_run_id_control(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
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
						const std::atomic<std::uint32_t> & global_run_id,
						const std::uint32_t your_run_id) mutable
					{
						subscription_callback->send(std::move(publication), global_run_id, your_run_id);
					},
					subscription_callback_->get_code_filename(),
					subscription_callback_->get_code_line());

				return high_way_mail_box_->send_may_fail(std::move(message));
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription{
			new SubscriptionProtectedHolderImpl{std::move(subscription_callback), std::move(highway_mail_box)}};
	} // create_send_may_fail_with_run_id_control

	static Subscription create_send_may_fail_no_run_id_control(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
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
					[subscription_callback = subscription_callback_, publication = std::move(publication)]() mutable
					{
						subscription_callback->send(std::move(publication));
					},
					subscription_callback_->get_code_filename(),
					subscription_callback_->get_code_line());

				return high_way_mail_box_->send_may_fail(std::move(message));
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription{
			new SubscriptionProtectedHolderImpl{std::move(subscription_callback), std::move(highway_mail_box)}};
	} // create_send_may_fail_no_run_id_control

	static Subscription create_send_may_blocked(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
		if (subscription_callback->has_run_id_control())
		{
			return create_send_may_blocked_with_run_id_control(
				std::move(subscription_callback),
				std::move(highway_mail_box));
		}
		else
		{
			return create_send_may_blocked_no_run_id_control(
				std::move(subscription_callback),
				std::move(highway_mail_box));
		}
	}

	static Subscription create_send_may_blocked_with_run_id_control(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
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
						const std::atomic<std::uint32_t> & global_run_id,
						const std::uint32_t your_run_id) mutable
					{
						subscription_callback->send(std::move(publication), global_run_id, your_run_id);
					},
					subscription_callback_->get_code_filename(),
					subscription_callback_->get_code_line());

				return high_way_mail_box_->send_may_blocked(std::move(message));
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription{
			new SubscriptionProtectedHolderImpl{std::move(subscription_callback), std::move(highway_mail_box)}};
	} // create_send_may_blocked_with_run_id_control

	static Subscription create_send_may_blocked_no_run_id_control(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
	{
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
					[subscription_callback = subscription_callback_, publication = std::move(publication)]() mutable
					{
						subscription_callback->send(std::move(publication));
					},
					subscription_callback_->get_code_filename(),
					subscription_callback_->get_code_line());

				return high_way_mail_box_->send_may_blocked(std::move(message));
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription{
			new SubscriptionProtectedHolderImpl{std::move(subscription_callback), std::move(highway_mail_box)}};
	} // create_send_may_blocked_no_run_id_control

	static Subscription create_direct_send(SubscriptionCallbackPtr<Publication> subscription_callback)
	{
		assert(subscription_callback);

		struct SubscriptionProtectedHolderImpl : public SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(SubscriptionCallbackPtr<Publication> subscription_callback)
				: subscription_callback_{std::move(subscription_callback)}
			{
			}

			bool operator()(Publication publication) override
			{
				return subscription_callback_->send(std::move(publication));
			}

			SubscriptionCallbackPtr<Publication> subscription_callback_;
		};

		return Subscription{new SubscriptionProtectedHolderImpl{std::move(subscription_callback)}};
	} // create_direct_send

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
		delete subscription_holder_;
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
	SubscriptionHolder * subscription_holder_{nullptr};
}; // Subscription

/**
 *  @brief ISubscribeHere
 *  Interface where future subscribers can subscribe
 */
template <typename Publication>
struct ISubscribeHere
{
	virtual ~ISubscribeHere() = default;

	virtual void subscribe(Subscription<Publication> && subscription) = 0;
};

template <typename Publication>
using ISubscribeHerePtr = std::shared_ptr<ISubscribeHere<Publication>>;

/**
 *  @brief IPublisher
 *  Interface through which you can publish
 */
template <typename Publication>
struct IPublisher
{
	virtual ~IPublisher() = default;

	virtual void publish(Publication publication) const = 0;
};

template <typename Publication>
using IPublisherPtr = std::shared_ptr<IPublisher<Publication>>;

/**
 * Subscribing to a channel
 *
 * @param subscribe_channel - channel to subscribe to
 * @param callback - publish handler callback
 * @param protector - subscription protection (a way to terminate a subscription)
 * @param highway_mailbox - executor mailbox on which the callback code should be executed
 * @param send_may_fail - is it permissible to skip (lose sight of) publications?
 *	(some tasks can be skipped if there is not enough RAM)
 * @param filename - file where the code is located
 * @param line - line in the file that contains the code
 */
template <typename Publication, typename R, typename P>
void subscribe(
	ISubscribeHere<Publication> & subscribe_channel,
	R && callback,
	P protector,
	IHighWayMailBoxPtr highway_mailbox,
	const bool send_may_fail = true,
	std::string filename = __FILE__,
	const unsigned int line = __LINE__)
{
	subscribe_channel.subscribe(hi::Subscription<Publication>::create(
		std::move(callback),
		std::move(protector),
		std::move(highway_mailbox),
		send_may_fail,
		std::move(filename),
		line));
}

/**
 * Subscribing to a channel
 *
 * @param subscribe_channel - channel to subscribe to
 * @param callback - publish handler callback
 * @param protector - subscription protection (a way to terminate a subscription)
 * @param highway_mailbox - executor mailbox on which the callback code should be executed
 * @param send_may_fail - is it permissible to skip (lose sight of) publications?
 *	(some tasks can be skipped if there is not enough RAM)
 * @param filename - file where the code is located
 * @param line - line in the file that contains the code
 */
template <typename Publication, typename R, typename P>
void subscribe(
	const ISubscribeHerePtr<Publication> & subscribe_channel,
	R && callback,
	P protector,
	IHighWayMailBoxPtr highway_mailbox,
	const bool send_may_fail = true,
	std::string filename = __FILE__,
	const unsigned int line = __LINE__)
{
	subscribe(
		*subscribe_channel,
		std::move(callback),
		std::move(protector),
		std::move(highway_mailbox),
		send_may_fail,
		std::move(filename),
		line);
}

/**
 * Subscribing to a channel. Direct send in publisher thread.
 *
 * @param subscribe_channel - channel to subscribe to
 * @param callback - publish handler callback
 * @param protector - subscription protection (a way to terminate a subscription)
 * @param filename - file where the code is located
 * @param line - line in the file that contains the code
 */
template <typename Publication, typename R, typename P>
void subscribe(
	ISubscribeHere<Publication> & subscribe_channel,
	R && callback,
	P protector,
	std::string filename = __FILE__,
	const unsigned int line = __LINE__)
{
	subscribe_channel.subscribe(
		hi::Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line));
}

/**
 * Subscribing to a channel. Direct send in publisher thread.
 *
 * @param subscribe_channel - channel to subscribe to
 * @param callback - publish handler callback
 * @param protector - subscription protection (a way to terminate a subscription)
 * @param filename - file where the code is located
 * @param line - line in the file that contains the code
 */
template <typename Publication, typename R, typename P>
void subscribe(
	const ISubscribeHerePtr<Publication> & subscribe_channel,
	R && callback,
	P protector,
	std::string filename = __FILE__,
	const unsigned int line = __LINE__)
{
	subscribe(*subscribe_channel, std::move(callback), std::move(protector), std::move(filename), line);
}

/**
 * @brief protect
 * Wrapping a subscription with an additional protector.
 * For example, to add an additional switch to a subscription.
 *
 * @param subscription - someone else's subscription
 * @param protector - your optional subscription switch
 * @note usage example:
 * https://github.com/DimaBond174/thread_highways/blob/main/tests/execution_tree/self-developing_execution_tree/src/test_self_develop.cpp
 * @note by analogy, you can create your custom subscriptions
 */
template <typename Publication, typename Protector>
Subscription<Publication> protect(Subscription<Publication> && subscription, Protector protector)
{
	struct SubscriptionProtectedHolderImpl : public Subscription<Publication>::SubscriptionHolder
	{
		SubscriptionProtectedHolderImpl(Subscription<Publication> && subscription, Protector protector)
			: subscription_{std::move(subscription)}
			, protector_{std::move(protector)}
		{
		}

		bool operator()(Publication publication) override
		{
			if (auto lock = protector_.lock())
			{
				return subscription_.send(std::move(publication));
			}
			return false;
		}

		Subscription<Publication> subscription_;
		Protector protector_;
	};

	return Subscription<Publication>{
		new SubscriptionProtectedHolderImpl{std::move(subscription), std::move(protector)}};
}

} // namespace hi

#endif // I_PUBLISHER_H
