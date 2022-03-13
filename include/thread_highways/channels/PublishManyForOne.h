#ifndef PUBLISH_MANY_FOR_ONE_H
#define PUBLISH_MANY_FOR_ONE_H

#include <thread_highways/channels/IPublishSubscribe.h>

namespace hi
{

template <typename Publication>
class PublishManyForOne : public IPublisher<Publication>
{
public:
	// Если требуется менять подписчика, то стоит рассмотреть PublishManyForMany
	PublishManyForOne(Subscription<Publication> && subscription)
		: subscription_{std::move(subscription)}
	{
	}

public: // IPublisher
	void publish(Publication publication) const override
	{
		subscription_.send(publication);
	}

private:
	const Subscription<Publication> subscription_;
}; // PublishManyForOne

} // namespace hi

#endif // PUBLISH_MANY_FOR_ONE_H
