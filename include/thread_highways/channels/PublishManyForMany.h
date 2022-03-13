#ifndef PUBLISH_MANY_FOR_MANY_H
#define PUBLISH_MANY_FOR_MANY_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <thread>

namespace hi
{

template <typename Publication>
class PublishManyForMany : public IPublisher<Publication>
{
public:
	PublishManyForMany(std::weak_ptr<PublishManyForMany<Publication>> self_weak)
		: self_weak_{std::move(self_weak)}
	{
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishManyForMany<Publication>> self_weak)
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
			const std::weak_ptr<PublishManyForMany<Publication>> self_weak_;
		};
		return std::make_shared<SubscribeHereImpl>(SubscribeHereImpl{self_weak_});
	}

	void subscribe(Subscription<Publication> && subscription)
	{
		subscriptions_safe_stack_.push(new Holder<Subscription<Publication>>{std::move(subscription)});
	}

	bool subscribers_exist()
	{
		return !!subscriptions_safe_stack_.access_stack();
	}

public: // IPublisher
	void publish(Publication publication) const override
	{
		for (Holder<Subscription<Publication>> * holder = subscriptions_safe_stack_.access_stack(); holder;
			 holder = holder->next_in_stack_)
		{
			holder->t_.send(publication);
		}
	}

private:
	const std::weak_ptr<PublishManyForMany<Publication>> self_weak_;
	ThreadSafeStack<Holder<Subscription<Publication>>> subscriptions_safe_stack_;
}; // PublishManyForMany

} // namespace hi

#endif // PUBLISH_MANY_FOR_MANY_H
