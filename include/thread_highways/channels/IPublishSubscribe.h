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

			void operator()(
				[[maybe_unused]] Publication publication,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (
					std::is_invocable_v<R, Publication, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication), global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Publication>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication));
				}
				else
				{
					safe_invoke_void(callback_, protector_);
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

	void send(
		Publication publication,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(std::move(publication), global_run_id, your_run_id);
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
		virtual void operator()(
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

template <typename Publication>
class Subscription
{
public:
	/*!
	 Фабрика подписок
	 @subscription_callback - куда передавать публикацию,нужен в shared_ptr т.к. постоянно копируется в Runnable
	 @highway_mail_box - куда постится Runnable(с захваченной публикацией и subscription_callback) для обработки
	 @send_may_fail - обязательна ли отправка: при обязательной отправке будет ждать свободных холдеров в
	 high_way_mail_box и тем самым может заблокироваться до тех пор пока не получит свободный холдер для отправки.

	 В Холдеры помещаются объекты и после этого можно их поместить в mail_box_ - это позволяет
	 контроллировать расход оперативной памяти.
	 Количество холдеров можно увеличить через метод IHighWay->set_capacity(N)
	*/
	static Subscription create(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mailbox,
		bool send_may_fail = true)
	{
		return send_may_fail ? create_send_may_fail(std::move(subscription_callback), std::move(highway_mailbox))
							 : create_send_may_blocked(std::move(subscription_callback), std::move(highway_mailbox));
	}

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

	static Subscription create_send_may_fail(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
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
	}

	static Subscription create_send_may_blocked(
		SubscriptionCallbackPtr<Publication> subscription_callback,
		IHighWayMailBoxPtr highway_mail_box)
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

/*!
	Обобщающая функция создания подписки
	@param subscribe_channel - канал на который следует подписаться
	@param callback - колбэк обработчика публикации
	@param protector - защита подписки (способ прекратить подписку)
	@param highway - экзекутор на котором должен выполняться код колбэка
	@param send_may_fail - можно пропустить отправку если на хайвее подписчика закончились холдеры сообщений
	@param filename - координаты кода для отладки
	@param line - координаты кода для отладки
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

	//	struct RunnableHolder
	//	{
	//		RunnableHolder(R && r)
	//			: r_{std::move(r)}
	//		{
	//		}

	//		void operator()(
	//			Publication publication,
	//			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
	//			[[maybe_unused]] const std::uint32_t your_run_id)
	//		{
	//			if constexpr (std::is_invocable_v<R, Publication, const std::atomic<std::uint32_t> &, const
	//std::uint32_t>)
	//			{
	//				r_(publication, global_run_id, your_run_id);
	//			}
	//			else
	//			{
	//				r_(publication);
	//			}
	//		}

	//		R r_;
	//	};

	//	auto subscription_callback = hi::SubscriptionCallback<Publication>::create(
	//		RunnableHolder{std::move(callback)},
	//		std::move(protector),
	//		std::move(filename),
	//		line);
	//	subscribe_channel.subscribe(
	//		hi::Subscription<Publication>::create(std::move(subscription_callback), std::move(highway_mailbox),
	//send_may_fail));
}

} // namespace hi

#endif // I_PUBLISHER_H
