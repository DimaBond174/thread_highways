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

	PublishManyForOne(Subscription<Publication> && subscription)
		: subscription_{std::move(subscription)}
	{
	}

	template <typename R, typename P>
	PublishManyForOne(
		R && callback,
		P protector,
		IHighWayMailBoxPtr highway_mailbox,
		const bool send_may_fail = true,
		std::string filename = __FILE__,
		const unsigned int line = __LINE__)
		: subscription_{Subscription<Publication>::create(
			std::move(callback),
			std::move(protector),
			std::move(highway_mailbox),
			send_may_fail,
			std::move(filename),
			line)}
	{
	}

	template <typename R, typename P>
	PublishManyForOne(R && callback, P protector, std::string filename = __FILE__, const unsigned int line = __LINE__)
		: subscription_{
			Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line)}
	{
	}

public: // IPublisher
	void publish(Publication publication) const override
	{
		subscription_.send(std::move(publication));
	}

private:
	const Subscription<Publication> subscription_;
}; // PublishManyForOne

} // namespace hi

#endif // PUBLISH_MANY_FOR_ONE_H
