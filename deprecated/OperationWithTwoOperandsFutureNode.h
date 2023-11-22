/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef OPERATIONWITHTWOOPERANDSFUTURENODE_H
#define OPERATIONWITHTWOOPERANDSFUTURENODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishManyForMany.h>
#include <thread_highways/channels/PublishManyForManyCanUnSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

namespace hi
{

template <typename Operand1, typename Operand2, typename Result>
class OperationWithTwoOperandsFutureNodeLogic
{
public:
	/**
	 * @brief The LaunchParameters struct
	 * The structure groups all incoming parameters for convenience.
	 * This is convenient because with an increase in the number of
	 * incoming parameters, you will not have to make changes to previously developed callbacks.
	 */
	struct LaunchParameters
	{
		// incoming data (publication from channel1)
		Operand1 operand1_;
		// incoming data (publication from channel2)
		Operand2 operand2_;
		// result publisher
		const std::reference_wrapper<IPublisher<Result>> result_publisher_;
		// accessor to base class of the execution tree node.
		const std::reference_wrapper<INode> node_;
		// identifier with which this highway works now
		const std::reference_wrapper<const std::atomic<std::uint32_t>> global_run_id_;
		// your_run_id - identifier with which this highway was running when this task started
		const std::uint32_t your_run_id_;
	};
	template <typename R, typename P>
	static std::shared_ptr<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line,
		std::shared_ptr<IPublisher<Result>> future_result_publisher)
	{
		struct OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl
			: public OperationWithTwoOperandsFutureNodeLogicHolder
		{
			OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				Operand1 operand1,
				Operand2 operand2,
				[[maybe_unused]] IPublisher<Result> & result_publisher,
				[[maybe_unused]] INode & node,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<R, LaunchParameters>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						LaunchParameters{
							std::move(operand1),
							std::move(operand2),
							result_publisher,
							node,
							global_run_id,
							your_run_id});
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   Operand1,
									   Operand2,
									   IPublisher<Result> &,
									   INode &,
									   const std::atomic<std::uint32_t> &,
									   const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(operand1),
						std::move(operand2),
						result_publisher,
						node,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Operand1, Operand2, IPublisher<Result> &, INode &>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(operand1),
						std::move(operand2),
						result_publisher,
						node);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   Operand1,
									   Operand2,
									   IPublisher<Result> &,
									   const std::atomic<std::uint32_t> &,
									   const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(operand1),
						std::move(operand2),
						result_publisher,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Operand1, Operand2, IPublisher<Result> &>)
				{
					safe_invoke_void(callback_, protector_, std::move(operand1), std::move(operand2), result_publisher);
				}
				else if constexpr (std::is_invocable_v<R, Operand1, Operand2>)
				{
					safe_invoke_void(callback_, protector_, std::move(operand1), std::move(operand2));
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && callback = *callback_;
					if constexpr (std::is_invocable_v<decltype(callback), LaunchParameters>)
					{
						safe_invoke_void(
							callback,
							protector_,
							LaunchParameters{
								std::move(operand1),
								std::move(operand2),
								result_publisher,
								node,
								global_run_id,
								your_run_id});
					}
					else if constexpr (std::is_invocable_v<
										   decltype(callback),
										   Operand1,
										   Operand2,
										   IPublisher<Result> &,
										   INode &,
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(operand1),
							std::move(operand2),
							result_publisher,
							node,
							global_run_id,
							your_run_id);
					}
					else if constexpr (
						std::is_invocable_v<decltype(callback), Operand1, Operand2, IPublisher<Result> &, INode &>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(operand1),
							std::move(operand2),
							result_publisher,
							node);
					}
					else if constexpr (std::is_invocable_v<
										   decltype(callback),
										   Operand1,
										   Operand2,
										   IPublisher<Result> &,
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(operand1),
							std::move(operand2),
							result_publisher,
							global_run_id,
							your_run_id);
					}
					else if constexpr (std::
										   is_invocable_v<decltype(callback), Operand1, Operand2, IPublisher<Result> &>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(operand1),
							std::move(operand2),
							result_publisher);
					}
					else if constexpr (std::is_invocable_v<decltype(callback), Operand1, Operand2>)
					{
						safe_invoke_void(callback, protector_, std::move(operand1), std::move(operand2));
					}
					else
					{
						// The callback signature must be one of the above
						assert(false);
					}
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
		return std::make_shared<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>>(
			new OperationWithTwoOperandsFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line,
			std::move(future_result_publisher));
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
		future_result_publisher_ = rhs.future_result_publisher_;
		delete subscription_callback_holder_;
		subscription_callback_holder_ = rhs.subscription_callback_holder_;
		rhs.subscription_callback_holder_ = nullptr;
		return *this;
	}

	void send(
		Operand1 operand1,
		Operand2 operand2,
		INode & node,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(
			std::move(operand1),
			std::move(operand2),
			*future_result_publisher_,
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
		if (auto it = std::dynamic_pointer_cast<PublishManyForManyCanUnSubscribe<Result>>(future_result_publisher_))
		{
			return it->subscribe_channel();
		}
		return nullptr;
	}

	struct OperationWithTwoOperandsFutureNodeLogicHolder
	{
		virtual ~OperationWithTwoOperandsFutureNodeLogicHolder() = default;
		virtual void operator()(
			Operand1 operand1,
			Operand2 operand2,
			IPublisher<Result> & result_publisher,
			[[maybe_unused]] INode & node,
			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
			[[maybe_unused]] const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	OperationWithTwoOperandsFutureNodeLogic(
		OperationWithTwoOperandsFutureNodeLogicHolder * subscription_holder,
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
	OperationWithTwoOperandsFutureNodeLogicHolder * subscription_callback_holder_{nullptr};
	std::shared_ptr<IPublisher<Result>> future_result_publisher_;
}; // OperationWithTwoOperandsFutureNodeLogic

template <typename Operand1, typename Operand2, typename Result>
using OperationWithTwoOperandsFutureNodeLogicPtr =
	std::shared_ptr<OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>>;

/*!
 * Узел в котором отрабатывает логика только когда получены публикации 2 операнда.
 * После отработки логики значения оберандов сбрасываются и всё повторяется.
 */
template <typename Operand1, typename Operand2, typename Result>
class OperationWithTwoOperandsFutureNode : public INode
{
public:
	OperationWithTwoOperandsFutureNode(
		std::weak_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> self_weak,
		OperationWithTwoOperandsFutureNodeLogicPtr<Operand1, Operand2, Result> future_node_logic,
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
	 * Фабрика создания узла OperationWithTwoOperandsFutureNode
	 * @param callback - логика обработки 2-х операндов
	 * @param protector - защита callback-а чтобы он вызывался только пока в этом ещё есть смысл
	 * (Например: если в callback обрабатываются поля объекта, то имеет смысл захватить std::weak_ptr на этот объект)
	 * @param highway - на каких мощностях будет запускаться логика callback, являются ли эти мощности многопоточными
	 * (Например: для ConcurrentHighWay и SingleThreadHighWay будут использоваться разные механизмы публикации
	 * результата)
	 * @param operand1_channel - Publisher значений операнда1
	 * @param operand2_channel - Publisher значений операнда2
	 * @param send_may_fail - можно ли пропускать публикации операндов
	 * @param filename - возможность сослаться на source code для помощи в отладке
	 * (Хайвеи могут логировать зависший/упавший код - вышеуказанная строка будет в логе)
	 * @param line - возможность сослаться на source code line для помощи в отладке
	 * @param current_executed_node_publisher - используется для публикации прогресса и активности работы узлов
	 * @param node_id - идентификатор этого узла чтобы отличать его в публикациях current_executed_node_publisher
	 * @param subscribers_can_unsubscribe - касается только хайвея который не highway->is_single_threaded(),
	 *	помогает выбрать между PublishManyForManyCanUnSubscribe и PublishManyForMany(второй работает без мьютексов)
	 */
	template <typename R, typename P>
	static std::shared_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> create(
		R && callback,
		P protector,
		std::shared_ptr<IHighWay> highway, //именно хайвей чтобы понять нужен ли ManyToMany паблишер
		ISubscribeHere<Operand1> & operand1_channel,
		ISubscribeHere<Operand2> & operand2_channel,
		const bool send_may_fail = false,
		std::string filename = __FILE__,
		const unsigned int line = __LINE__,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		const std::uint32_t node_id = 0,
		bool subscribers_can_unsubscribe = false)
	{
		auto publisher = [&]() -> std::shared_ptr<IPublisher<Result>>
		{
			if (highway->is_single_threaded())
			{
				return make_self_shared<PublishOneForMany<Result>>();
			}
			if (subscribers_can_unsubscribe)
			{
				return make_self_shared<PublishManyForManyCanUnSubscribe<Result>>();
			}
			return make_self_shared<PublishManyForMany<Result>>();
		}();

		auto future_node_logic = OperationWithTwoOperandsFutureNodeLogic<Operand1, Operand2, Result>::create(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line,
			std::move(publisher));

		auto node = make_self_shared<hi::OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>>(
			std::move(future_node_logic),
			highway->mailbox(),
			std::move(current_executed_node_publisher),
			node_id);

		node->add_operand1_channel(operand1_channel, send_may_fail);
		node->add_operand2_channel(operand2_channel, send_may_fail);

		return node;
	} // create

	void add_operand1_channel(ISubscribeHere<Operand1> & where_to_subscribe, bool send_may_fail = false)
	{
		auto subscription_callback = hi::SubscriptionCallback<Operand1>::create(
			[this, inode_weak = self_weak_](
				Operand1 operand1,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id) mutable
			{
				if (auto inode = inode_weak.lock())
				{
					std::lock_guard lg{state_protector_};
					if (!operand2_)
					{
						operand1_.emplace(std::move(operand1));
						return;
					}
					future_node_logic_
						->send(std::move(operand1), std::move(*operand2_), *inode, global_run_id, your_run_id);
					operand1_.reset();
					operand2_.reset();
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);

		where_to_subscribe.subscribe(
			hi::Subscription<Operand1>::create(std::move(subscription_callback), highway_mailbox_, send_may_fail));
	}

	void add_operand2_channel(ISubscribeHere<Operand2> & where_to_subscribe, bool send_may_fail = false)
	{
		auto subscription_callback = hi::SubscriptionCallback<Operand2>::create(
			[this, inode_weak = self_weak_](
				Operand2 operand2,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id)
			{
				if (auto inode = inode_weak.lock())
				{
					std::lock_guard lg{state_protector_};
					if (!operand1_)
					{
						operand2_.emplace(std::move(operand2));
						return;
					}
					future_node_logic_
						->send(std::move(*operand1_), std::move(operand2), *inode, global_run_id, your_run_id);
					operand1_.reset();
					operand2_.reset();
				}
			},
			self_weak_,
			__FILE__,
			__LINE__);

		where_to_subscribe.subscribe(
			hi::Subscription<Operand2>::create(std::move(subscription_callback), highway_mailbox_, send_may_fail));
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<OperationWithTwoOperandsFutureNode<Operand1, Operand2, Result>> self_weak_;
	const OperationWithTwoOperandsFutureNodeLogicPtr<Operand1, Operand2, Result> future_node_logic_;
	const IHighWayMailBoxPtr highway_mailbox_;

	std::mutex state_protector_;
	std::optional<Operand1> operand1_;
	std::optional<Operand2> operand2_;

}; // FutureNode

} // namespace hi

#endif // OPERATIONWITHTWOOPERANDSFUTURENODE_H
