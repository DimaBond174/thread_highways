#ifndef PUBLISH_MANY_FOR_ONE_H
#define PUBLISH_MANY_FOR_ONE_H

#include <thread_highways/channels/IPublishSubscribe.h>

namespace hi
{

template <typename Publication>
class PublishManyForOne : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	// Если требуется менять подписчика, то стоит рассмотреть PublishManyForMany
	PublishManyForOne(Subscription<Publication> && subscription)
		: subscription_{std::move(subscription)}
	{
	}

	template <typename R>
	PublishManyForOne(
		std::shared_ptr<IHighWay> highway,
		R && callback,
		bool send_may_fail = true,
		std::string filename = __FILE__,
		unsigned int line = __LINE__)
		: subscription_{
			create_subscription(std::move(highway), std::move(callback), send_may_fail, std::move(filename), line)}
	{
	}

public: // IPublisher
	void publish(Publication publication) const override
	{
		subscription_.send(std::move(publication));
	}

private:
	template <typename R>
	Subscription<Publication> create_subscription(
		std::shared_ptr<IHighWay> highway,
		R && callback,
		bool send_may_fail,
		std::string filename,
		unsigned int line)
	{
		struct RunnableHolder
		{
			RunnableHolder(R && r)
				: r_{std::move(r)}
			{
			}

			void operator()(
				Publication publication,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id)
			{
				if constexpr (
					std::is_invocable_v<R, Publication, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					r_(publication, global_run_id, your_run_id);
				}
				else
				{
					r_(publication);
				}
			}

			R r_;
		};

		auto subscription_callback = hi::SubscriptionCallback<Publication>::create(
			RunnableHolder{std::move(callback)},
			highway->protector(),
			filename,
			line);

		return hi::Subscription<Publication>::create(
			std::move(subscription_callback),
			highway->mailbox(),
			send_may_fail);
	}

private:
	const Subscription<Publication> subscription_;
}; // PublishManyForOne

} // namespace hi

#endif // PUBLISH_MANY_FOR_ONE_H
