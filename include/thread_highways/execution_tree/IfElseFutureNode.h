#ifndef IFELSE_FutureNode_H
#define IFELSE_FutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

namespace hi
{

template <typename Parameter, typename IfResult, typename ElseResult>
class IfElseFutureNodeLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line)
	{
		struct IfElseFutureNodeLogicProtectedHolderImpl : public IfElseFutureNodeLogicHolder
		{
			IfElseFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				Parameter publication,
				IPublisher<IfResult> & if_result_publisher,
				IPublisher<ElseResult> & else_result_publisher) override
			{
				safe_invoke_void(
					callback_,
					protector_,
					std::move(publication),
					if_result_publisher,
					else_result_publisher);
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
		return std::make_shared<IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>>(
			new IfElseFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line);
	}

	~IfElseFutureNodeLogic()
	{
		delete subscription_callback_holder_;
	}
	IfElseFutureNodeLogic(const IfElseFutureNodeLogic & rhs) = delete;
	IfElseFutureNodeLogic & operator=(const IfElseFutureNodeLogic & rhs) = delete;
	IfElseFutureNodeLogic(IfElseFutureNodeLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_if_result_publisher_{rhs.future_if_result_publisher_}
		, future_else_result_publisher_{rhs.future_else_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	IfElseFutureNodeLogic & operator=(IfElseFutureNodeLogic && rhs)
	{
		if (this == &rhs)
			return *this;
		filename_ = std::move(rhs.callback_);
		line_ = rhs.line_;
		subscription_callback_holder_ = rhs.subscription_callback_holder_;
		future_if_result_publisher_ = rhs.future_if_result_publisher_;
		future_else_result_publisher_ = rhs.future_else_result_publisher_;
		rhs.subscription_callback_holder_ = nullptr;
		return *this;
	}

	void send(Parameter publication) const
	{
		(*subscription_callback_holder_)(
			std::move(publication),
			*future_if_result_publisher_,
			*future_else_result_publisher_);
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

	ISubscribeHerePtr<IfResult> subscribe_if_result_channel()
	{
		return future_if_result_publisher_->subscribe_channel();
	}

	ISubscribeHerePtr<ElseResult> subscribe_else_result_channel()
	{
		return future_else_result_publisher_->subscribe_channel();
	}

	struct IfElseFutureNodeLogicHolder
	{
		virtual ~IfElseFutureNodeLogicHolder() = default;
		virtual void operator()(
			Parameter publication,
			IPublisher<IfResult> & if_result_publisher,
			IPublisher<ElseResult> & else_result_publisher) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	IfElseFutureNodeLogic(IfElseFutureNodeLogicHolder * subscription_holder, std::string filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, subscription_callback_holder_{subscription_holder}
		, future_if_result_publisher_{make_self_shared<PublishOneForMany<IfResult>>()}
		, future_else_result_publisher_{make_self_shared<PublishOneForMany<ElseResult>>()}
	{
		assert(subscription_callback_holder_);
		assert(future_if_result_publisher_);
		assert(future_else_result_publisher_);
	}

private:
	std::string filename_;
	unsigned int line_;
	IfElseFutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<PublishOneForMany<IfResult>> future_if_result_publisher_;
	std::shared_ptr<PublishOneForMany<ElseResult>> future_else_result_publisher_;
}; // IfElseFutureNodeLogic

template <typename Parameter, typename IfResult, typename ElseResult>
using IfElseFutureNodeLogicPtr = std::shared_ptr<IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>>;

template <typename Parameter, typename IfResult, typename ElseResult>
class IfElseFutureNode : public INode
{
public:
	IfElseFutureNode(
		std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> self_weak,
		IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	IfElseFutureNode(
		std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> self_weak,
		IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	Subscription<Parameter> subscription(bool send_may_fail = true) const
	{
		return send_may_fail ? subscription_send_may_fail() : subscription_send_may_blocked();
	}

	Subscription<Parameter> subscription_send_may_fail() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
				IHighWayMailBoxPtr high_way_mail_box)
				: future_node_logic_{std::move(future_node_logic)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
			{
			}

			bool operator()(Parameter publication) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				auto message = hi::Runnable::create(
					[this, subscription_callback = future_node_logic_, publication = std::move(publication)](
						const std::atomic<std::uint32_t> &,
						const std::uint32_t) mutable
					{
						subscription_callback->send(std::move(publication));
					},
					future_node_logic_->get_code_filename(),
					future_node_logic_->get_code_line());
				return high_way_mail_box_->send_may_fail(std::move(message));
			}

			IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription<Parameter>{new SubscriptionProtectedHolderImpl{future_node_logic_, high_way_mail_box_}};
	}

	Subscription<Parameter> subscription_send_may_blocked() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
				IHighWayMailBoxPtr high_way_mail_box)
				: future_node_logic_{std::move(future_node_logic)}
				, high_way_mail_box_{std::move(high_way_mail_box)}
			{
			}

			bool operator()(Parameter publication) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				auto message = hi::Runnable::create(
					[this, subscription_callback = future_node_logic_, publication = std::move(publication)](
						const std::atomic<std::uint32_t> &,
						const std::uint32_t) mutable
					{
						subscription_callback->send(std::move(publication));
					},
					future_node_logic_->get_code_filename(),
					future_node_logic_->get_code_line());

				return high_way_mail_box_->send_may_blocked(std::move(message));
			}

			IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription<Parameter>{new SubscriptionProtectedHolderImpl{future_node_logic_, high_way_mail_box_}};
	}

	ISubscribeHerePtr<IfResult> if_result_channel()
	{
		return future_node_logic_->subscribe_if_result_channel();
	}

	ISubscribeHerePtr<ElseResult> else_result_channel()
	{
		return future_node_logic_->subscribe_else_result_channel();
	}

private:
	const std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> self_weak_;
	IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
	IHighWayMailBoxPtr high_way_mail_box_;
}; // IfElseFutureNode

} // namespace hi

#endif // IFELSE_FutureNode_H
