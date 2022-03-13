#ifndef PUBLISH_ONE_FOR_MANY_H
#define PUBLISH_ONE_FOR_MANY_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace hi
{

// реализация исходя из того что паблишер работает в 1м потоке
template <typename Publication>
class PublishOneForMany : public IPublisher<Publication>
{
public:
	PublishOneForMany(std::weak_ptr<PublishOneForMany<Publication>> self_weak)
		: self_weak_{std::move(self_weak)}
	{
	}

	ISubscribeHerePtr<Publication> subscribe_channel()
	{
		struct SubscribeHereImpl : public ISubscribeHere<Publication>
		{
			SubscribeHereImpl(std::weak_ptr<PublishOneForMany<Publication>> self_weak)
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
			const std::weak_ptr<PublishOneForMany<Publication>> self_weak_;
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
		check_local_thread();
		subscriptions_safe_stack_.move_to(local_work_stack_);
		SingleThreadStack<Holder<Subscription<Publication>>> local_tmp_stack;
		while (Holder<Subscription<Publication>> * holder = local_work_stack_.pop())
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
		local_tmp_stack.swap(local_work_stack_);
	}

private:
	void check_local_thread() const
	{
		if (!local_thread_id_)
		{
			local_thread_id_.emplace(std::this_thread::get_id());
		}
		if (*local_thread_id_ != std::this_thread::get_id())
		{
			throw std::logic_error("PublishOneForMany: must use single thread only for this operation");
		}
	}

private:
	const std::weak_ptr<PublishOneForMany<Publication>> self_weak_;
	mutable ThreadSafeStack<Holder<Subscription<Publication>>> subscriptions_safe_stack_;
	mutable SingleThreadStack<Holder<Subscription<Publication>>> local_work_stack_;
	mutable std::optional<std::thread::id> local_thread_id_;
}; // PublishOneForMany

} // namespace hi

#endif // PUBLISH_ONE_FOR_MANY_H
