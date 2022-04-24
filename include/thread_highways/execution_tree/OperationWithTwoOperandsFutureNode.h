#ifndef OPERATIONWITHTWOOPERANDSFUTURENODE_H
#define OPERATIONWITHTWOOPERANDSFUTURENODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace hi
{

template <typename Operand1, typename Operand2, typename Result>
class OperationWithTwoOperandsFutureNodeLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line)
	{
		struct OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl
			: public OperationWithTwoOperandsFutureNodeLogicHolder
		{
			OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(Operand1 operand1, Operand2 operand2, IPublisher<Result> & result_publisher) override
			{
				safe_invoke_void(callback_, protector_, std::move(operand1), std::move(operand2), result_publisher);
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
		return std::make_shared<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>>(
			new OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line);
	}

	~OperationWithTwoOperandsFutureNodeLogic()
	{
		delete subscription_callback_holder_;
	}
	OperationWithTwoOperandsFutureNodeLogic(const OperationWithTwoOperandsFutureNodeLogic & rhs) = delete;
	OperationWithTwoOperandsFutureNodeLogic & operator=(const OperationWithTwoOperandsFutureNodeLogic & rhs) = delete;
	OperationWithTwoOperandsFutureNodeLogic(OperationWithTwoOperandsFutureNodeLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_result_publisher_{rhs.future_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	OperationWithTwoOperandsFutureNodeLogic & operator=(OperationWithTwoOperandsFutureNodeLogic && rhs)
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

	void send(Operand1 operand1, Operand2 operand2) const
	{
		(*subscription_callback_holder_)(std::move(operand1), std::move(operand2), *future_result_publisher_);
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

	struct OperationWithTwoOperandsFutureNodeLogicHolder
	{
		virtual ~OperationWithTwoOperandsFutureNodeLogicHolder() = default;
		virtual void operator()(Operand1 operand1, Operand2 operand2, IPublisher<Result> & result_publisher) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	OperationWithTwoOperandsFutureNodeLogic(
		OperationWithTwoOperandsFutureNodeLogicHolder * subscription_holder,
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
	OperationWithTwoOperandsFutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<PublishOneForMany<Result>> future_result_publisher_;
}; // OperationWithTwoOperandsFutureNodeLogic

template <typename Operand1, typename Operand2, typename Result>
using OperationWithTwoOperandsFutureNodeLogicPtr =
	std::shared_ptr<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>>;

// todo сделать вариант для корутин где AggregatingBundle будет внутри корутины
template <typename Operand1, typename Operand2, typename Result>
class OperationWithTwoOperandsFutureNode : public INode
{
public:
	OperationWithTwoOperandsFutureNode(
		std::weak_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> self_weak,
		OperationWithTwoOperandsFutureNodeLogicPtr<Operand1, Operand2, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	OperationWithTwoOperandsFutureNode(
		std::weak_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> self_weak,
		OperationWithTwoOperandsFutureNodeLogicPtr<Operand1, Operand2, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	void add_operand1_channel(ISubscribeHere<Operand1> & where_to_subscribe)
	{
		auto subscription_callback = hi::SubscriptionCallback<Operand1>::create(
			[this](Operand1 operand1, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				if (!operand2_)
				{
					operand1_.emplace(std::move(operand1));
					publish_progress_state(true, 50);
					return;
				}
				future_node_logic_->send(std::move(operand1), std::move(*operand2_));
				operand1_.reset();
				operand2_.reset();
				publish_progress_state(false, 100);
			},
			self_weak_,
			__FILE__,
			__LINE__);

		where_to_subscribe.subscribe(
			hi::Subscription<Operand1>::create(std::move(subscription_callback), high_way_mail_box_));
	}

	void add_operand2_channel(ISubscribeHere<Operand2> & where_to_subscribe)
	{
		auto subscription_callback = hi::SubscriptionCallback<Operand2>::create(
			[this](Operand2 operand2, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				if (!operand1_)
				{
					operand2_.emplace(std::move(operand2));
					publish_progress_state(true, 50);
					return;
				}
				future_node_logic_->send(std::move(*operand1_), std::move(operand2));
				operand1_.reset();
				operand2_.reset();
				publish_progress_state(false, 100);
			},
			self_weak_,
			__FILE__,
			__LINE__);

		where_to_subscribe.subscribe(
			hi::Subscription<Operand2>::create(std::move(subscription_callback), high_way_mail_box_));
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> self_weak_;
	const OperationWithTwoOperandsFutureNodeLogicPtr<Operand1, Operand2, Result> future_node_logic_;
	const IHighWayMailBoxPtr high_way_mail_box_;

	std::optional<Operand1> operand1_;
	std::optional<Operand2> operand2_;

}; // FutureNode

} // namespace hi

#endif // OPERATIONWITHTWOOPERANDSFUTURENODE_H
