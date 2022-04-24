#ifndef PUBLISH_MANY_FOR_MANY_CAN_UnSubscribe_H
#define PUBLISH_MANY_FOR_MANY_CAN_UnSubscribe_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace hi
{

template <typename Publication>
class PublishManyForManyCanUnSubscribe : public IPublisher<Publication>
{
public:
	typedef Publication PublicationType;

	PublishManyForManyCanUnSubscribe(std::weak_ptr<PublishManyForManyCanUnSubscribe<Publication>> self_weak)
		: self_weak_{std::move(self_weak)}
	{
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishManyForManyCanUnSubscribe<Publication>> self_weak)
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
			const std::weak_ptr<PublishManyForManyCanUnSubscribe<Publication>> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

	void subscribe(Subscription<Publication> && subscription)
	{
		std::lock_guard lg{mutex_};
		subscriptions_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
	}

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
		local_tmp_stack.swap(subscriptions_stack_);
	}

private:
	const std::weak_ptr<PublishManyForManyCanUnSubscribe<Publication>> self_weak_;
	mutable std::mutex mutex_;
	mutable SingleThreadStack<Holder<Subscription<Publication>>> subscriptions_stack_;
}; // PublishManyForManyCanUnSubscribe

} // namespace hi

#endif // PUBLISH_MANY_FOR_MANY_CAN_UnSubscribe_H
