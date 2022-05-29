#ifndef PUBLISH_MANY_FOR_ONE_H
#define PUBLISH_MANY_FOR_ONE_H

#include <thread_highways/channels/IPublishSubscribe.h>

namespace hi
{

/**
 * @brief PublishManyForOne
 * Implementing the Sink Pattern.
 * Multiple threads publish for one subscriber.
 * (implementation based on the fact that the publisher works in any thread).
 * @note subscriber will not be deleted
 */
template <typename Publication>
class PublishManyForOne : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	/**
	 * @brief PublishManyForOne
	 * Constructor
	 * @param subscription - subscriber, chosen one
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 */
	PublishManyForOne(Subscription<Publication> && subscription)
		: subscription_{std::move(subscription)}
	{
	}

	/**
	 * @brief PublishManyForOne
	 * Constructor
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
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 * @note below there is an option to create a subscription without rescheduling sending via highway
	 */
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

	/**
	 * @brief PublishManyForOne
	 * Constructor
	 * @param callback - where to send the publication
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the subscription is considered broken
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note usage examples:
	 * https://github.com/DimaBond174/thread_highways/blob/main/examples/channels/publish_many_for_one/src/channels_publish_many_for_one.cpp
	 * https://github.com/DimaBond174/thread_highways/blob/main/tests/channels/src/test_publush_many_for_one.cpp
	 */
	template <typename R, typename P>
	PublishManyForOne(R && callback, P protector, std::string filename = __FILE__, const unsigned int line = __LINE__)
		: subscription_{
			Subscription<Publication>::create(std::move(callback), std::move(protector), std::move(filename), line)}
	{
	}

public: // IPublisher
	/**
	 * @brief publish
	 * submit publication
	 * @param publication
	 * @note can be called from any thread
	 */
	void publish(Publication publication) const override
	{
		subscription_.send(std::move(publication));
	}

private:
	const Subscription<Publication> subscription_;
}; // PublishManyForOne

} // namespace hi

#endif // PUBLISH_MANY_FOR_ONE_H
