/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef RESULTWAITResultWaitFutureNode_H
#define RESULTWAITResultWaitFutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/execution_tree/IExecutionTree.h>
#include <thread_highways/tools/make_self_shared.h>

#include <condition_variable>
#include <mutex>
#include <optional>

namespace hi
{

/*!
 Узел ничего никуда уже не шлёт, ничего не делает.
 Позволяет подписаться на результат, дождаться результата
	и хранить результат.
 Позволяет сменить текущую подписку на результат
	- начать ждать результат с другого узла (применяется при динамическом перестроении execution tree).
 Существует только одна подписка а результат (нельзя подписаться на результаты с нескольких узлов).

 Есть встроенный ExecutionTree в котором удобно хранить узлы дерева пока они нужны для достижения результата.

 ResultWaitFutureNode удобен для юнит тестов - можно
  блокируясь на ResultWaitFutureNode::get_result() дождаться результата.
*/
template <typename Result>
class ResultWaitFutureNode : public INode
{
public:
	ResultWaitFutureNode(
		std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
		IHighWayMailBoxPtr highway_mailbox,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		std::uint32_t node_id = 0)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, highway_mailbox_{std::move(highway_mailbox)}
	{
	}

	Subscription<Result> subscription(bool send_may_fail = true)
	{
		return send_may_fail ? subscription_send_may_fail() : subscription_send_may_blocked();
	}

	Subscription<Result> subscription_send_may_fail()
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Result>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<bool> protector,
				std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
				IHighWayMailBoxPtr highway_mailbox)
				: protector_{std::move(protector)}
				, self_weak_{std::move(self_weak)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Result publication) override
			{
				if (auto lock = protector_.lock())
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
					return highway_mailbox_->send_may_fail(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<bool> protector_;
			const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		std::lock_guard<std::mutex> lk{mutex_};
		protector_ = std::make_shared<bool>();
		return Subscription<Result>{
			new SubscriptionProtectedHolderImpl{std::weak_ptr(protector_), self_weak_, highway_mailbox_}};
	}

	Subscription<Result> subscription_send_may_blocked()
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Result>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<bool> protector,
				std::weak_ptr<ResultWaitFutureNode<Result>> self_weak,
				IHighWayMailBoxPtr highway_mailbox)
				: protector_{std::move(protector)}
				, self_weak_{std::move(self_weak)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Result publication) override
			{
				if (auto lock = protector_.lock())
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
					return highway_mailbox_->send_may_blocked(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<bool> protector_;
			const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		std::lock_guard<std::mutex> lk{mutex_};
		protector_ = std::make_shared<bool>();
		return Subscription<Result>{
			new SubscriptionProtectedHolderImpl{std::weak_ptr(protector_), self_weak_, highway_mailbox_}};
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

	void reset_result()
	{
		std::lock_guard<std::mutex> lk{mutex_};
		result_.reset();
	}

	ExecutionTree & execution_tree()
	{
		return execution_tree_;
	}

private:
	void set_result(Result && result)
	{
		{
			std::lock_guard<std::mutex> lk{mutex_};
			result_ = std::make_shared<Result>(std::move(result));
		}
		cv_.notify_one();
	}

private:
	const std::weak_ptr<ResultWaitFutureNode<Result>> self_weak_;
	const IHighWayMailBoxPtr highway_mailbox_;

	std::mutex mutex_;
	std::condition_variable cv_;
	std::shared_ptr<Result> result_;
	std::shared_ptr<bool> protector_;

	ExecutionTree execution_tree_;
}; // ResultWaitFutureNode

} // namespace hi

#endif // RESULTWAITResultWaitFutureNode_H
