#ifndef AGGREGATINGFUTURENODE_H
#define AGGREGATINGFUTURENODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishManyForMany.h>
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
		unsigned int line,
		std::shared_ptr<IPublisher<Result>> future_result_publisher)
	{
		struct AggregatingFutureNodeLogicProtectedHolderImpl : public AggregatingFutureNodeLogicHolder
		{
			AggregatingFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				const std::uint32_t operand_id,
				Operand operand_value,
				AggregatingBundle & aggregating_bundle,
				[[maybe_unused]] IPublisher<Result> & result_publisher,
				[[maybe_unused]] const std::uint32_t operands_count,
				[[maybe_unused]] INode & node,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<
								  R,
								  std::uint32_t,
								  Operand,
								  AggregatingBundle &,
								  IPublisher<Result> &,
								  std::uint32_t,
								  INode &,
								  const std::atomic<std::uint32_t> &,
								  const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						operand_id,
						std::move(operand_value),
						aggregating_bundle,
						result_publisher,
						operands_count,
						node,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   std::uint32_t,
									   Operand,
									   AggregatingBundle &,
									   IPublisher<Result> &,
									   std::uint32_t,
									   INode &>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						operand_id,
						std::move(operand_value),
						aggregating_bundle,
						result_publisher,
						operands_count,
						node);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   std::uint32_t,
									   Operand,
									   AggregatingBundle &,
									   IPublisher<Result> &,
									   std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						operand_id,
						std::move(operand_value),
						aggregating_bundle,
						result_publisher,
						operands_count);
				}
				else if constexpr (
					std::is_invocable_v<R, std::uint32_t, Operand, AggregatingBundle &, IPublisher<Result> &>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						operand_id,
						std::move(operand_value),
						aggregating_bundle,
						result_publisher);
				}
				else if constexpr (std::is_invocable_v<R, std::uint32_t, Operand, AggregatingBundle &>)
				{
					safe_invoke_void(callback_, protector_, operand_id, std::move(operand_value), aggregating_bundle);
				}
				else
				{
					// The callback signature must be one of the above
					assert(false);
				}
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
			line,
			std::move(future_result_publisher));
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
		const std::uint32_t operands_count,
		INode & node,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(
			operand_id,
			std::move(operand_value),
			aggregating_bundle,
			*future_result_publisher_,
			operands_count,
			node,
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
		if (auto it = std::dynamic_pointer_cast<PublishManyForMany<Result>>(future_result_publisher_))
		{
			return it->subscribe_channel();
		}
		if (auto it = std::dynamic_pointer_cast<PublishOneForMany<Result>>(future_result_publisher_))
		{
			return it->subscribe_channel();
		}
		return nullptr;
	}

	struct AggregatingFutureNodeLogicHolder
	{
		virtual ~AggregatingFutureNodeLogicHolder() = default;
		virtual void operator()(
			const std::uint32_t operand_id,
			Operand operand_value,
			AggregatingBundle & aggregating_bundle,
			IPublisher<Result> & result_publisher,
			const std::uint32_t operands_count,
			INode & node,
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	AggregatingFutureNodeLogic(
		AggregatingFutureNodeLogicHolder * subscription_holder,
		std::string filename,
		unsigned int line,
		std::shared_ptr<IPublisher<Result>> future_result_publisher)
		: filename_{std::move(filename)}
		, line_{line}
		, subscription_callback_holder_{subscription_holder}
		, future_result_publisher_{std::move(future_result_publisher)}
	{
		assert(subscription_callback_holder_);
		assert(future_result_publisher_);
	}

private:
	std::string filename_;
	unsigned int line_;
	AggregatingFutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<IPublisher<Result>> future_result_publisher_;
}; // AggregatingFutureNodeLogic

template <typename Operand, typename AggregatingBundle, typename Result>
using AggregatingFutureNodeLogicPtr = std::shared_ptr<AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>>;

template <typename Operand, typename AggregatingBundle, typename Result>
class AggregatingFutureNode : public INode
{
public:
	AggregatingFutureNode(
		std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak,
		AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic,
		IHighWayMailBoxPtr highway_mailbox,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		std::uint32_t node_id = 0)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, highway_mailbox_{std::move(highway_mailbox)}
	{
	}

	/*!
	 * Фабрика создания узла AggregatingFutureNode
	 * @param callback - логика обработки
	 * @param protector - защита callback-а чтобы он вызывался только пока в этом ещё есть смысл
	 * (Например: если в callback обрабатываются поля объекта, то имеет смысл захватить std::weak_ptr на этот объект)
	 * @param highway - на каких мощностях будет запускаться логика callback, являются ли эти мощности многопоточными
	 * (Например: для ConcurrentHighWay и SingleThreadHighWay будут использоваться разные механизмы публикации
	 * результата)
	 * @param filename - возможность сослаться на source code для помощи в отладке
	 * (Хайвеи могут логировать зависший/упавший код - вышеуказанная строка будет в логе)
	 * @param line - возможность сослаться на source code line для помощи в отладке
	 * @param current_executed_node_publisher - используется для публикации прогресса и активности работы узлов
	 * @param node_id - идентификатор этого узла чтобы отличать его в публикациях current_executed_node_publisher
	 */
	template <typename R, typename P>
	static std::shared_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> create(
		R && callback,
		P protector,
		std::shared_ptr<IHighWay> highway, //именно хайвей чтобы понять нужен ли ManyToMany паблишер
		std::string filename = __FILE__,
		const unsigned int line = __LINE__,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		const std::uint32_t node_id = 0)
	{
		auto result_publisher = [&]() -> std::shared_ptr<IPublisher<Result>>
		{
			if (highway->is_single_threaded())
			{
				return make_self_shared<PublishOneForMany<Result>>();
			}
			return make_self_shared<PublishManyForMany<Result>>();
		}();

		auto future_node_logic = AggregatingFutureNodeLogic<Operand, AggregatingBundle, Result>::create(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line,
			std::move(result_publisher));

		return make_self_shared<AggregatingFutureNode<Operand, AggregatingBundle, Result>>(
			std::move(future_node_logic),
			highway->mailbox(),
			std::move(current_executed_node_publisher),
			node_id);
	} // create

	void add_operand_channel(ISubscribeHere<Operand> & where_to_subscribe, bool send_may_fail = true)
	{
		const std::uint32_t operand_id = operands_count_.fetch_add(1, std::memory_order_relaxed);
		hi::subscribe(
			where_to_subscribe,
			[this, inode_weak = self_weak_, operand_id](
				Operand operand_value,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id) mutable
			{
				if (auto inode = inode_weak.lock())
				{
					future_node_logic_->send(
						operand_id,
						std::move(operand_value),
						aggregating_bundle_,
						operands_count_.load(std::memory_order_acquire),
						*inode,
						global_run_id,
						your_run_id);
				}
			},
			self_weak_,
			highway_mailbox_,
			send_may_fail,
			__FILE__,
			__LINE__);
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<AggregatingFutureNode<Operand, AggregatingBundle, Result>> self_weak_;
	const AggregatingFutureNodeLogicPtr<Operand, AggregatingBundle, Result> future_node_logic_;
	const IHighWayMailBoxPtr highway_mailbox_;

	AggregatingBundle aggregating_bundle_;
	std::atomic<std::uint32_t> operands_count_{0};
}; // FutureNode

} // namespace hi

#endif // AGGREGATINGFUTURENODE_H
