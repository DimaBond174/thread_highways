#ifndef FutureNodeWithProgressPublisherWITHPROGRESSPUBLISHER_H
#define FutureNodeWithProgressPublisherWITHPROGRESSPUBLISHER_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <functional>

namespace hi
{

/*
Как FutureNode, но в логике можно проверять надо ли уже останавливать выполнение
 и можно публиковать ход исполнения
*/
template <typename Parameter, typename Result>
class FutureNodeWithProgressPublisherLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<FutureNodeWithProgressPublisherLogic<Parameter, Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line)
	{
		struct FutureNodeWithProgressPublisherLogicProtectedHolderImpl
			: public FutureNodeWithProgressPublisherLogicHolder
		{
			FutureNodeWithProgressPublisherLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				Parameter publication,
				IPublisher<Result> & result_publisher,
				const PublishProgressStateCallback & publish_progress_state_callback,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id) override
			{
				safe_invoke_void(
					callback_,
					protector_,
					std::move(publication),
					result_publisher,
					publish_progress_state_callback,
					global_run_id,
					your_run_id);
			}

			[[nodiscard]] bool alive() override
			{
				if (auto lock = protector_.lock())
				{
					return true;
				}
				return false;
			}

			R callback_;
			P protector_;
		};
		return std::make_shared<FutureNodeWithProgressPublisherLogic<Parameter, Result>>(
			new FutureNodeWithProgressPublisherLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line);
	}

	~FutureNodeWithProgressPublisherLogic()
	{
		delete subscription_callback_holder_;
	}
	FutureNodeWithProgressPublisherLogic(const FutureNodeWithProgressPublisherLogic & rhs) = delete;
	FutureNodeWithProgressPublisherLogic & operator=(const FutureNodeWithProgressPublisherLogic & rhs) = delete;
	FutureNodeWithProgressPublisherLogic(FutureNodeWithProgressPublisherLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_result_publisher_{rhs.future_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	FutureNodeWithProgressPublisherLogic & operator=(FutureNodeWithProgressPublisherLogic && rhs)
	{
		if (this == &rhs)
			return *this;
		filename_ = std::move(rhs.callback_);
		line_ = rhs.line_;
		subscription_callback_holder_ = rhs.subscription_callback_holder_;
		future_result_publisher_ = rhs.future_result_publisher_;
		rhs.subscription_callback_holder_ = nullptr;
		return *this;
	}

	void send(
		Parameter publication,
		const PublishProgressStateCallback & publish_progress_state_callback,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(
			std::move(publication),
			*future_result_publisher_,
			publish_progress_state_callback,
			global_run_id,
			your_run_id);
	}

	bool alive()
	{
		return subscription_callback_holder_->alive();
	}

	std::string get_code_filename()
	{
		return filename_;
	}

	unsigned int get_code_line()
	{
		return line_;
	}

	ISubscribeHerePtr<Result> subscribe_result_channel()
	{
		return future_result_publisher_->subscribe_channel();
	}

	struct FutureNodeWithProgressPublisherLogicHolder
	{
		virtual ~FutureNodeWithProgressPublisherLogicHolder() = default;
		virtual void operator()(
			Parameter publication,
			IPublisher<Result> & result_publisher,
			const PublishProgressStateCallback & publish_progress_state_callback,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	FutureNodeWithProgressPublisherLogic(
		FutureNodeWithProgressPublisherLogicHolder * subscription_holder,
		std::string filename,
		unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, subscription_callback_holder_{subscription_holder}
		, future_result_publisher_{make_self_shared<PublishOneForMany<Result>>()}
	{
		assert(subscription_callback_holder_);
		assert(future_result_publisher_);
	}

private:
	std::string filename_;
	unsigned int line_;
	FutureNodeWithProgressPublisherLogicHolder * subscription_callback_holder_;
	std::shared_ptr<PublishOneForMany<Result>> future_result_publisher_;
}; // FutureNodeWithProgressPublisherLogic

template <typename Parameter, typename Result>
using FutureNodeWithProgressPublisherLogicPtr =
	std::shared_ptr<FutureNodeWithProgressPublisherLogic<Parameter, Result>>;

template <typename Parameter, typename Result>
class FutureNodeWithProgressPublisher : public INode
{
public:
	FutureNodeWithProgressPublisher(
		std::weak_ptr<FutureNodeWithProgressPublisher<Parameter, Result>> self_weak,
		FutureNodeWithProgressPublisherLogicPtr<Parameter, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	FutureNodeWithProgressPublisher(
		std::weak_ptr<FutureNodeWithProgressPublisher<Parameter, Result>> self_weak,
		FutureNodeWithProgressPublisherLogicPtr<Parameter, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	template <bool send_may_fail = true>
	Subscription<Parameter> subscription() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				FutureNodeWithProgressPublisherLogicPtr<Parameter, Result> future_node_logic,
				IHighWayMailBoxPtr high_way_mail_box,
				std::weak_ptr<FutureNodeWithProgressPublisher<Parameter, Result>> self_weak)
				: future_node_logic_{std::move(future_node_logic)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
				, self_weak_{std::move(self_weak)}
			{
			}

			bool operator()(Parameter publication) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				auto message = Runnable::create(
					[subscription_callback = future_node_logic_,
					 publication = std::move(publication),
					 self_weak = self_weak_](
						const std::atomic<std::uint32_t> & global_run_id,
						const std::uint32_t your_run_id) mutable
					{
						subscription_callback->send(
							std::move(publication),
							[self_weak](bool in_progress, std::uint16_t achieved_progress)
							{
								safe_invoke_void(
									&INode::publish_progress_state,
									self_weak,
									in_progress,
									achieved_progress);
							},
							global_run_id,
							your_run_id);
					},
					future_node_logic_->get_code_filename(),
					future_node_logic_->get_code_line());
				if constexpr (send_may_fail)
				{
					return high_way_mail_box_->send_may_fail(std::move(message));
				}
				else
				{
					return high_way_mail_box_->send_may_blocked(std::move(message));
				}
			}

			const FutureNodeWithProgressPublisherLogicPtr<Parameter, Result> future_node_logic_;
			const IHighWayMailBoxPtr high_way_mail_box_;
			const std::weak_ptr<FutureNodeWithProgressPublisher<Parameter, Result>> self_weak_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{future_node_logic_, high_way_mail_box_, self_weak_}};
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<FutureNodeWithProgressPublisher<Parameter, Result>> self_weak_;
	FutureNodeWithProgressPublisherLogicPtr<Parameter, Result> future_node_logic_;
	IHighWayMailBoxPtr high_way_mail_box_;
}; // FutureNodeWithProgressPublisher

} // namespace hi

#endif // FutureNodeWithProgressPublisherWITHPROGRESSPUBLISHER_H
