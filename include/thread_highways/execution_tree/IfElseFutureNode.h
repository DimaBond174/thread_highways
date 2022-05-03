#ifndef IFELSE_FutureNode_H
#define IFELSE_FutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishManyForMany.h>
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
		unsigned int line,
		std::shared_ptr<IPublisher<ElseResult>> if_result_publisher,
		std::shared_ptr<IPublisher<ElseResult>> else_result_publisher)
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
				[[maybe_unused]] IPublisher<ElseResult> & else_result_publisher,
				[[maybe_unused]] INode & node,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<
								  R,
								  Parameter,
								  IPublisher<IfResult> &,
								  IPublisher<ElseResult> &,
								  INode &,
								  const std::atomic<std::uint32_t> &,
								  const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						if_result_publisher,
						else_result_publisher,
						node,
						global_run_id,
						your_run_id);
				}
				else if constexpr (
					std::is_invocable_v<R, Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &, INode &>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						if_result_publisher,
						else_result_publisher,
						node);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   Parameter,
									   IPublisher<IfResult> &,
									   IPublisher<ElseResult> &,
									   const std::atomic<std::uint32_t> &,
									   const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						if_result_publisher,
						else_result_publisher,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Parameter, IPublisher<IfResult> &, IPublisher<ElseResult> &>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						if_result_publisher,
						else_result_publisher);
				}
				else if constexpr (std::is_invocable_v<R, Parameter, IPublisher<IfResult> &>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication), if_result_publisher);
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
		return std::make_shared<IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>>(
			new IfElseFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line,
			std::move(if_result_publisher),
			std::move(else_result_publisher));
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

	void send(
		Parameter publication,
		INode & node,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(
			std::move(publication),
			*future_if_result_publisher_,
			*future_else_result_publisher_,
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

	ISubscribeHerePtr<IfResult> subscribe_if_result_channel()
	{
		if (auto it = std::dynamic_pointer_cast<PublishManyForMany<IfResult>>(future_if_result_publisher_))
		{
			return it->subscribe_channel();
		}
		if (auto it = std::dynamic_pointer_cast<PublishOneForMany<IfResult>>(future_if_result_publisher_))
		{
			return it->subscribe_channel();
		}
		return nullptr;
	}

	ISubscribeHerePtr<ElseResult> subscribe_else_result_channel()
	{
		if (auto it = std::dynamic_pointer_cast<PublishManyForMany<ElseResult>>(future_else_result_publisher_))
		{
			return it->subscribe_channel();
		}
		if (auto it = std::dynamic_pointer_cast<PublishOneForMany<ElseResult>>(future_else_result_publisher_))
		{
			return it->subscribe_channel();
		}
		return nullptr;
	}

	struct IfElseFutureNodeLogicHolder
	{
		virtual ~IfElseFutureNodeLogicHolder() = default;
		virtual void operator()(
			Parameter publication,
			IPublisher<IfResult> & if_result_publisher,
			[[maybe_unused]] IPublisher<ElseResult> & else_result_publisher,
			[[maybe_unused]] INode & node,
			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
			[[maybe_unused]] const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	IfElseFutureNodeLogic(
		IfElseFutureNodeLogicHolder * subscription_holder,
		std::string filename,
		unsigned int line,
		std::shared_ptr<IPublisher<ElseResult>> if_result_publisher,
		std::shared_ptr<IPublisher<ElseResult>> else_result_publisher)
		: filename_{std::move(filename)}
		, line_{line}
		, subscription_callback_holder_{subscription_holder}
		, future_if_result_publisher_{std::move(if_result_publisher)}
		, future_else_result_publisher_{std::move(else_result_publisher)}
	{
		assert(subscription_callback_holder_);
		assert(future_if_result_publisher_);
		assert(future_else_result_publisher_);
	}

private:
	std::string filename_;
	unsigned int line_;
	IfElseFutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<IPublisher<IfResult>> future_if_result_publisher_;
	std::shared_ptr<IPublisher<ElseResult>> future_else_result_publisher_;
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
	 * Фабрика создания узла IfElseFutureNode
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
	static std::shared_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> create(
		R && callback,
		P protector,
		std::shared_ptr<IHighWay> highway, //именно хайвей чтобы понять нужен ли ManyToMany паблишер
		std::string filename = __FILE__,
		const unsigned int line = __LINE__,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		const std::uint32_t node_id = 0)
	{
		auto if_result_publisher = [&]() -> std::shared_ptr<IPublisher<IfResult>>
		{
			if (highway->is_single_threaded())
			{
				return make_self_shared<PublishOneForMany<IfResult>>();
			}
			return make_self_shared<PublishManyForMany<IfResult>>();
		}();

		auto else_result_publisher = [&]() -> std::shared_ptr<IPublisher<ElseResult>>
		{
			if (highway->is_single_threaded())
			{
				return make_self_shared<PublishOneForMany<ElseResult>>();
			}
			return make_self_shared<PublishManyForMany<ElseResult>>();
		}();

		auto future_node_logic = IfElseFutureNodeLogic<Parameter, IfResult, ElseResult>::create(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line,
			std::move(if_result_publisher),
			std::move(else_result_publisher));

		return make_self_shared<IfElseFutureNode<Parameter, IfResult, ElseResult>>(
			std::move(future_node_logic),
			highway->mailbox(),
			std::move(current_executed_node_publisher),
			node_id);
	} // create

	Subscription<Parameter> subscription(bool send_may_fail = true) const
	{
		return send_may_fail ? subscription_send_may_fail() : subscription_send_may_blocked();
	}

	Subscription<Parameter> subscription_send_may_fail() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> inode_weak,
				IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
				IHighWayMailBoxPtr highway_mailbox)
				: inode_weak_{std::move(inode_weak)}
				, future_node_logic_{std::move(future_node_logic)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Parameter publication) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				if (auto lock = inode_weak_.lock())
				{
					auto message = hi::Runnable::create(
						[this,
						 inode_weak = inode_weak_,
						 subscription_callback = future_node_logic_,
						 publication = std::move(publication)](
							const std::atomic<std::uint32_t> & global_run_id,
							const std::uint32_t your_run_id) mutable
						{
							if (auto inode = inode_weak.lock())
							{
								subscription_callback->send(std::move(publication), *inode, global_run_id, your_run_id);
							}
						},
						future_node_logic_->get_code_filename(),
						future_node_logic_->get_code_line());
					return highway_mailbox_->send_may_fail(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> inode_weak_;
			const IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{self_weak_, future_node_logic_, highway_mailbox_}};
	}

	Subscription<Parameter> subscription_send_may_blocked() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> inode_weak,
				IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic,
				IHighWayMailBoxPtr highway_mailbox)
				: inode_weak_{std::move(inode_weak)}
				, future_node_logic_{std::move(future_node_logic)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Parameter publication) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				if (auto lock = inode_weak_.lock())
				{
					auto message = hi::Runnable::create(
						[this,
						 inode_weak = inode_weak_,
						 subscription_callback = future_node_logic_,
						 publication = std::move(publication)](
							const std::atomic<std::uint32_t> & global_run_id,
							const std::uint32_t your_run_id) mutable
						{
							if (auto inode = inode_weak.lock())
							{
								subscription_callback->send(std::move(publication), *inode, global_run_id, your_run_id);
							}
						},
						future_node_logic_->get_code_filename(),
						future_node_logic_->get_code_line());
					return highway_mailbox_->send_may_blocked(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<IfElseFutureNode<Parameter, IfResult, ElseResult>> inode_weak_;
			const IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{self_weak_, future_node_logic_, highway_mailbox_}};
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
	const IfElseFutureNodeLogicPtr<Parameter, IfResult, ElseResult> future_node_logic_;
	const IHighWayMailBoxPtr highway_mailbox_;
}; // IfElseFutureNode

} // namespace hi

#endif // IFELSE_FutureNode_H
