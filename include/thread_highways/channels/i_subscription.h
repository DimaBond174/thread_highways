/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_CHANNELS_ISUBSCRIPTION_H
#define THREADS_HIGHWAYS_CHANNELS_ISUBSCRIPTION_H

#include <thread_highways/highways/highway.h>
#include <thread_highways/tools/make_self_shared.h>

#include <memory>
#include <optional>

namespace hi
{

template <typename Publication>
struct ISubscription
{
	virtual ~ISubscription() = default;
	virtual bool send(Publication publication) = 0;
};

template <typename R, typename Publication>
inline bool send_with_params(
	R & callback,
	[[maybe_unused]] Publication publication,
	[[maybe_unused]] const std::atomic<bool> & keep_execution)
{
	if constexpr (std::is_invocable_v<R, Publication, const std::atomic<bool> &>)
	{
		callback(std::move(publication), keep_execution);
	}
	else if constexpr (std::is_invocable_v<R, Publication>)
	{
		callback(std::move(publication));
	}
	else if constexpr (std::is_invocable_v<R, const std::atomic<bool> &>)
	{
		callback(keep_execution);
	}
	else if constexpr (std::is_invocable_v<R>)
	{
		callback();
	}
	else if constexpr (can_be_dereferenced<R &>::value)
	{
		auto && r = *callback;
		if constexpr (std::is_invocable_v<decltype(r), Publication, const std::atomic<bool> &>)
		{
			r(std::move(publication), keep_execution);
		}
		else if constexpr (std::is_invocable_v<decltype(r), Publication>)
		{
			r(std::move(publication));
		}
		else if constexpr (std::is_invocable_v<decltype(r), const std::atomic<bool> &>)
		{
			r(keep_execution);
		}
		else
		{
			r();
		}
	}
	else
	{
		return false;
	}
	return true;
}

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription(R && callback)
{
	struct Subscription : public ISubscription<Publication>
	{
		Subscription(R && callback)
			: callback_{std::move(callback)}
		{
		}
		bool send(Publication publication) final
		{
			callback_(std::move(publication));
			return true;
		}

		R callback_;
	};

	return std::make_shared<Subscription>(std::move(callback));
}

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription(
	R && callback,
	HighWayProxyPtr highway,
	const char * filename,
	unsigned int line,
	bool send_may_fail = true)
{
	if (send_may_fail)
	{
		struct Subscription : public ISubscription<Publication>
		{
			Subscription(
				std::weak_ptr<Subscription> self,
				R && callback,
				HighWayProxyPtr highway,
				const char * filename,
				unsigned int line)
				: self_{std::move(self)}
				, callback_{std::move(callback)}
				, highway_{std::move(highway)}
				, filename_{filename}
				, line_{line}
			{
			}

			bool send(Publication publication) final
			{
				auto runnable = Runnable::create(
					[&, p = std::move(publication)](const std::atomic<bool> & keep_execution) mutable
					{
						if (!send_with_params<R, Publication>(callback_, std::move(p), keep_execution))
						{
							throw Exception(
								"Error: subscription callback must take parameters: Publication publication, "
								"[[maybe_unused]] const std::atomic<bool>& keep_execution",
								filename_,
								line_);
						}
					},
					self_,
					filename_,
					line_);
				return highway_->try_execute(std::move(runnable));
			}

			const std::weak_ptr<Subscription> self_;
			R callback_;
			const HighWayProxyPtr highway_;
			const char * filename_;
			const unsigned int line_;
		};

		return make_self_shared<Subscription>(std::move(callback), std::move(highway), filename, line);
	}
	else
	{
		struct Subscription : public ISubscription<Publication>
		{
			Subscription(
				std::weak_ptr<Subscription> self,
				R && callback,
				HighWayProxyPtr highway,
				const char * filename,
				unsigned int line)
				: self_{std::move(self)}
				, callback_{std::move(callback)}
				, highway_{std::move(highway)}
				, filename_{filename}
				, line_{line}
			{
			}

			bool send(Publication publication) final
			{
				auto runnable = Runnable::create(
					[&, p = std::move(publication)](const std::atomic<bool> & keep_execution) mutable
					{
						if (!send_with_params<R, Publication>(callback_, std::move(p), keep_execution))
						{
							throw Exception(
								"Error: subscription callback must take parameters: Publication publication, "
								"[[maybe_unused]] const std::atomic<bool>& keep_execution",
								filename_,
								line_);
						}
					},
					self_,
					filename_,
					line_);
				return highway_->execute(std::move(runnable));
			}

			const std::weak_ptr<Subscription> self_;
			R callback_;
			const HighWayProxyPtr highway_;
			const char * filename_;
			const unsigned int line_;
		};

		return make_self_shared<Subscription>(std::move(callback), std::move(highway), filename, line);
	} // if
} // create_subscription

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription(
	R && callback,
	std::shared_ptr<HighWay> highway,
	const char * filename,
	unsigned int line,
	bool send_may_fail = true)
{
	return create_subscription<Publication, R>(std::move(callback), make_proxy(highway), filename, line, send_may_fail);
}

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription_for_new_only(R && callback)
{
	struct Subscription : public ISubscription<Publication>
	{
		Subscription(R && callback)
			: callback_{std::move(callback)}
		{
		}
		bool send(Publication publication) final
		{
			if (last_.has_value() && *last_ == publication)
			{
				return true;
			}
			callback_(publication);
			last_.emplace(std::move(publication));
			return true;
		}

		R callback_;
		std::optional<Publication> last_;
	};

	return std::make_shared<Subscription>(std::move(callback));
} // create_subscription_for_new_only

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription_for_new_only(
	R && callback,
	HighWayProxyPtr highway,
	const char * filename,
	unsigned int line,
	bool send_may_fail = true)
{
	if (send_may_fail)
	{
		struct Subscription : public ISubscription<Publication>
		{
			Subscription(
				std::weak_ptr<Subscription> self,
				R && callback,
				HighWayProxyPtr highway,
				const char * filename,
				unsigned int line)
				: self_{std::move(self)}
				, callback_{std::move(callback)}
				, highway_{std::move(highway)}
				, filename_{filename}
				, line_{line}
			{
			}

			bool send(Publication publication) final
			{
				if (last_.has_value() && *last_ == publication)
				{
					return true;
				}
				last_ = publication;

				auto runnable = Runnable::create(
					[&, p = std::move(publication)](const std::atomic<bool> & keep_execution) mutable
					{
						if (!send_with_params<R, Publication>(callback_, std::move(p), keep_execution))
						{
							throw Exception(
								"Error: subscription callback must take parameters: Publication publication, "
								"[[maybe_unused]] const std::atomic<bool>& keep_execution",
								filename_,
								line_);
						}
					},
					self_,
					filename_,
					line_);
				return highway_->try_execute(std::move(runnable));
			}

			const std::weak_ptr<Subscription> self_;
			R callback_;
			const HighWayProxyPtr highway_;
			const char * filename_;
			const unsigned int line_;
			std::optional<Publication> last_;
		};

		return make_self_shared<Subscription>(std::move(callback), std::move(highway), filename, line);
	}
	else
	{
		struct Subscription : public ISubscription<Publication>
		{
			Subscription(
				std::weak_ptr<Subscription> self,
				R && callback,
				HighWayProxyPtr highway,
				const char * filename,
				unsigned int line)
				: self_{std::move(self)}
				, callback_{std::move(callback)}
				, highway_{std::move(highway)}
				, filename_{filename}
				, line_{line}
			{
			}

			bool send(Publication publication) final
			{
				if (last_.has_value() && *last_ == publication)
				{
					return true;
				}
				last_ = publication;
				auto runnable = Runnable::create(
					[&, p = std::move(publication)](const std::atomic<bool> & keep_execution) mutable
					{
						if (!send_with_params<R, Publication>(callback_, std::move(p), keep_execution))
						{
							throw Exception(
								"Error: subscription callback must take parameters: Publication publication, "
								"[[maybe_unused]] const std::atomic<bool>& keep_execution",
								filename_,
								line_);
						}
					},
					self_,
					filename_,
					line_);
				return highway_->execute(std::move(runnable));
			}

			const std::weak_ptr<Subscription> self_;
			R callback_;
			const HighWayProxyPtr highway_;
			const char * filename_;
			const unsigned int line_;
			std::optional<Publication> last_;
		};

		return make_self_shared<Subscription>(std::move(callback), std::move(highway), filename, line);
	} // if
} // create_subscription_for_new_only

template <typename Publication, typename R>
inline std::shared_ptr<ISubscription<Publication>> create_subscription_for_new_only(
	R && callback,
	std::shared_ptr<HighWay> highway,
	const char * filename,
	unsigned int line,
	bool send_may_fail = true)
{
	return create_subscription_for_new_only<Publication, R>(
		std::move(callback),
		make_proxy(highway),
		filename,
		line,
		send_may_fail);
}

/**
 *  @brief ISubscribeHere
 *  Interface where future subscribers can subscribe
 */
template <typename Publication>
struct ISubscribeHere
{
	virtual ~ISubscribeHere() = default;

	// std::shared_ptr<ISubscription<Publication>> должен хранить подписчик
	//  - его удаление означает окончание подписки
	virtual void subscribe(std::weak_ptr<ISubscription<Publication>> subscription) = 0;

	template <typename R>
	std::shared_ptr<ISubscription<Publication>> subscribe(R && callback, bool for_new_only)
	{
		auto subscription = for_new_only ? create_subscription_for_new_only<Publication, R>(std::move(callback))
										 : create_subscription<Publication, R>(std::move(callback));
		subscribe(subscription);
		return subscription;
	}

	template <typename R>
	std::shared_ptr<ISubscription<Publication>> subscribe(
		R && callback,
		HighWayProxyPtr highway,
		const char * filename,
		unsigned int line,
		bool send_may_fail = true,
		bool for_new_only = false)
	{
		auto subscription = for_new_only ? create_subscription_for_new_only<Publication, R>(
								std::move(callback),
								std::move(highway),
								filename,
								line,
								send_may_fail)
										 : create_subscription<Publication, R>(
											 std::move(callback),
											 std::move(highway),
											 filename,
											 line,
											 send_may_fail);
		subscribe(subscription);
		return subscription;
	}

	template <typename R>
	std::shared_ptr<ISubscription<Publication>> subscribe(
		R && callback,
		std::shared_ptr<HighWay> highway,
		const char * filename,
		unsigned int line,
		bool send_may_fail = true,
		bool for_new_only = false)
	{
		auto highway_weak = std::make_shared<hi::HighWayProxy>(std::move(highway));
		auto subscription = for_new_only ? create_subscription_for_new_only<Publication, R>(
								std::move(callback),
								std::move(highway_weak),
								filename,
								line,
								send_may_fail)
										 : create_subscription<Publication, R>(
											 std::move(callback),
											 std::move(highway_weak),
											 filename,
											 line,
											 send_may_fail);
		subscribe(subscription);
		return subscription;
	}
};

template <typename Publication>
using ISubscribeHerePtr = std::shared_ptr<ISubscribeHere<Publication>>;

} // namespace hi

#endif // THREADS_HIGHWAYS_CHANNELS_ISUBSCRIPTION_H
