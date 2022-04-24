#ifndef RESULTWAITResultWaitFutureNode_H
#define RESULTWAITResultWaitFutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/execution_tree/ExecutionTree.h>
#include <thread_highways/tools/make_self_shared.h>

#include <condition_variable>
#include <mutex>
#include <optional>

namespace hi
{

/*
Ничего никуда уже не шлёт, но позволяет дождаться результата
и хранит результат
*/
template <typename Result>
class ResultWaitFutureNode : public INode
{
public:
	ResultWaitFutureNode(std::weak_ptr<ResultWaitFutureNode<Result>> self_weak, IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	ResultWaitFutureNode(
		std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	Subscription<Result> subscription(bool send_may_fail = true) const
	{
		return send_may_fail ? subscription_send_may_fail() : subscription_send_may_blocked();
	}

	Subscription<Result> subscription_send_may_fail() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Result>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
				IHighWayMailBoxPtr high_way_mail_box)
				: self_weak_{std::move(self_weak)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
			{
			}

			bool operator()(Result publication) override
			{
				auto message = hi::Runnable::create(
					[publication = std::move(publication),
					 self_weak = self_weak_](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
					{
						if (auto self = self_weak.lock())
						{
							self->publish_progress_state(false, 100);
							self->set_result(std::move(publication));
						}
					},
					__FILE__,
					__LINE__);
				return high_way_mail_box_->send_may_fail(std::move(message));
			}

			const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
			const IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription<Result>{new SubscriptionProtectedHolderImpl{self_weak_, high_way_mail_box_}};
	}

	Subscription<Result> subscription_send_may_blocked() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Result>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
				IHighWayMailBoxPtr high_way_mail_box)
				: self_weak_{std::move(self_weak)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
			{
			}

			bool operator()(Result publication) override
			{
				auto message = hi::Runnable::create(
					[publication = std::move(publication),
					 self_weak = self_weak_](const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
					{
						if (auto self = self_weak.lock())
						{
							self->publish_progress_state(false, 100);
							self->set_result(std::move(publication));
						}
					},
					__FILE__,
					__LINE__);
				return high_way_mail_box_->send_may_blocked(std::move(message));
			}

			const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
			const IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription<Result>{new SubscriptionProtectedHolderImpl{self_weak_, high_way_mail_box_}};
	}

	bool result_ready()
	{
		bool re = false;
		if (mutex_.try_lock())
		{
			re = !!result_;
			mutex_.unlock();
		}
		return re;
	}

	std::shared_ptr<Result> get_result()
	{
		std::unique_lock<std::mutex> lk{mutex_};
		cv_.wait(
			lk,
			[this]
			{
				return !!result_;
			});
		return result_;
	}

	// Сохранить дерево исполнения, результатом которого будет result_
	void store_parent_execution_tree(ExecutionTree && parent_execution_tree)
	{
		ExecutionTree parent_execution_tree_local{std::move(parent_execution_tree)};
		std::lock_guard<std::mutex> lk{mutex_};
		if (!result_)
		{
			parent_execution_tree_.emplace(std::move(parent_execution_tree_local));
		}
	}

private:
	void set_result(Result && result)
	{
		{
			std::lock_guard<std::mutex> lk{mutex_};
			result_ = std::make_shared<Result>(std::move(result));
			parent_execution_tree_.reset();
		}
		cv_.notify_one();
	}

private:
	const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
	IHighWayMailBoxPtr high_way_mail_box_;

	std::mutex mutex_;
	std::condition_variable cv_;
	std::shared_ptr<Result> result_;

	// дерево самоликвидируется как только результат будет заполнен
	std::optional<ExecutionTree> parent_execution_tree_;
}; // ResultWaitFutureNode

} // namespace hi

#endif // RESULTWAITResultWaitFutureNode_H
