#ifndef AGGREGATINGFUTURENODE_H
#define AGGREGATINGFUTURENODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace hi
{

template <typename Operand, typename AggregatingBundle, typename Result>
class AggregatingFutureNodeLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line)
	{
		struct AggregatingFutureNodeLogicProtectedHolderImpl : public AggregatingFutureNodeLogicHolder
		{
			AggregatingFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				std::uint32_t operand_id,
				Operand operand_value,
				AggregatingBundle & aggregating_bundle,
				std::uint32_t operands_count,
				IPublisher<Result> & result_publisher) override
			{
				safe_invoke_void(
					callback_,
					protector_,
					operand_id,
					std::move(operand_value),
					aggregating_bundle,
					operands_count,
					result_publisher);
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
		return std::make_shared<AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>>(
			new AggregatingFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line);
	}

	~AggregatingFutureNodeLogic()
	{
		delete subscription_callback_holder_;
	}
	AggregatingFutureNodeLogic(const AggregatingFutureNodeLogic & rhs) = delete;
	AggregatingFutureNodeLogic & operator=(const AggregatingFutureNodeLogic & rhs) = delete;
	AggregatingFutureNodeLogic(AggregatingFutureNodeLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_result_publisher_{rhs.future_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	AggregatingFutureNodeLogic & operator=(AggregatingFutureNodeLogic && rhs)
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
		const std::uint32_t operand_id,
		Operand operand_value,
		AggregatingBundle & aggregating_bundle,
		const std::uint32_t operands_count) const
	{
		(*subscription_callback_holder_)(
			operand_id,
			std::move(operand_value),
			aggregating_bundle,
			operands_count,
			*future_result_publisher_);
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

	struct AggregatingFutureNodeLogicHolder
	{
		virtual ~AggregatingFutureNodeLogicHolder() = default;
		virtual void operator()(
			std::uint32_t operand_id,
			Operand operand_value,
			AggregatingBundle & aggregating_bundle,
			std::uint32_t operands_count,
			IPublisher<Result> & result_publisher) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	AggregatingFutureNodeLogic(
		AggregatingFutureNodeLogicHolder * subscription_holder,
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
	AggregatingFutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<PublishOneForMany<Result>> future_result_publisher_;
}; // AggregatingFutureNodeLogic

template <typename Operand, typename AggregatingBundle, typename Result>
using AggregatingFutureNodeLogicPtr = std::shared_ptr<AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>>;

// todo сделать вариант для корутин где AggregatingBundle будет внутри корутины
template <typename Operand, typename AggregatingBundle, typename Result>
class AggregatingFutureNode : public INode
{
public:
	AggregatingFutureNode(
		std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak,
		AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	AggregatingFutureNode(
		std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak,
		AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	AggregatingFutureNode(
		std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak,
		AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic,
		AggregatingBundle && aggregating_bundle,
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
		, aggregating_bundle_{std::move(aggregating_bundle)}
	{
	}

	AggregatingFutureNode(
		std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak,
		AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic,
		AggregatingBundle && aggregating_bundle,
		IHighWayMailBoxPtr high_way_mail_box,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher,
		std::uint32_t node_id)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
		, aggregating_bundle_{std::move(aggregating_bundle)}
	{
	}

	void add_operand_channel(ISubscribeHere<Operand> & where_to_subscribe)
	{
		const std::uint32_t operand_id = operands_count_.fetch_add(1, std::memory_order_relaxed);
		auto subscription_callback = hi::SubscriptionCallback<Operand>::create(
			[this, operand_id](Operand operand_value, const std::atomic<std::uint32_t> &, const std::uint32_t) mutable
			{
				future_node_logic_->send(
					operand_id,
					std::move(operand_value),
					aggregating_bundle_,
					operands_count_.load(std::memory_order_acquire));
			},
			self_weak_,
			__FILE__,
			__LINE__);

		where_to_subscribe.subscribe(
			hi::Subscription<Operand>::create(std::move(subscription_callback), high_way_mail_box_));
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak_;
	const AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic_;
	const IHighWayMailBoxPtr high_way_mail_box_;

	AggregatingBundle aggregating_bundle_;
	std::atomic<std::uint32_t> operands_count_{0};
}; // FutureNode

} // namespace hi

#endif // AGGREGATINGFUTURENODE_H
