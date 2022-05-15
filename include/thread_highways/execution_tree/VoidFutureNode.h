/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef VoidFutureNode_H
#define VoidFutureNode_H

#include <thread_highways/execution_tree/FutureNode.h>

namespace hi
{

template <typename Result>
class VoidFutureNodeLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<VoidFutureNodeLogic<Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line,
		std::shared_ptr<IPublisher<Result>> future_result_publisher)
	{
		struct VoidFutureNodeLogicProtectedHolderImpl : public VoidFutureNodeLogicHolder
		{
			VoidFutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				[[maybe_unused]] IPublisher<Result> & result_publisher,
				[[maybe_unused]] INode & node,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<
								  R,
								  IPublisher<Result> &,
								  INode &,
								  const std::atomic<std::uint32_t> &,
								  const std::uint32_t>)
				{
					safe_invoke_void(callback_, protector_, result_publisher, node, global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, IPublisher<Result> &, INode &>)
				{
					safe_invoke_void(callback_, protector_, result_publisher, node);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   IPublisher<Result> &,
									   const std::atomic<std::uint32_t> &,
									   const std::uint32_t>)
				{
					safe_invoke_void(callback_, protector_, result_publisher, global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, IPublisher<Result> &>)
				{
					safe_invoke_void(callback_, protector_, result_publisher);
				}
				else if constexpr (std::is_invocable_v<R>)
				{
					safe_invoke_void(callback_, protector_);
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && callback = *callback_;
					if constexpr (std::is_invocable_v<
									  decltype(callback),
									  IPublisher<Result> &,
									  INode &,
									  const std::atomic<std::uint32_t> &,
									  const std::uint32_t>)
					{
						safe_invoke_void(callback, protector_, result_publisher, node, global_run_id, your_run_id);
					}
					else if constexpr (std::is_invocable_v<decltype(callback), IPublisher<Result> &, INode &>)
					{
						safe_invoke_void(callback, protector_, result_publisher, node);
					}
					else if constexpr (std::is_invocable_v<
										   decltype(callback),
										   IPublisher<Result> &,
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						safe_invoke_void(callback, protector_, result_publisher, global_run_id, your_run_id);
					}
					else if constexpr (std::is_invocable_v<decltype(callback), IPublisher<Result> &>)
					{
						safe_invoke_void(callback, protector_, result_publisher);
					}
					else
					{
						safe_invoke_void(callback, protector_);
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
		return std::make_shared<VoidFutureNodeLogic<Result>>(
			new VoidFutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line,
			std::move(future_result_publisher));
	}

	~VoidFutureNodeLogic()
	{
		delete subscription_callback_holder_;
	}
	VoidFutureNodeLogic(const VoidFutureNodeLogic & rhs) = delete;
	VoidFutureNodeLogic & operator=(const VoidFutureNodeLogic & rhs) = delete;
	VoidFutureNodeLogic(VoidFutureNodeLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_result_publisher_{rhs.future_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	VoidFutureNodeLogic & operator=(VoidFutureNodeLogic && rhs)
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

	void send(INode & node, const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(*future_result_publisher_, node, global_run_id, your_run_id);
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

	struct VoidFutureNodeLogicHolder
	{
		virtual ~VoidFutureNodeLogicHolder() = default;
		virtual void operator()(
			IPublisher<Result> & result_publisher,
			[[maybe_unused]] INode & node,
			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
			[[maybe_unused]] const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	VoidFutureNodeLogic(
		VoidFutureNodeLogicHolder * subscription_holder,
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
	VoidFutureNodeLogicHolder * subscription_callback_holder_{nullptr};
	std::shared_ptr<IPublisher<Result>> future_result_publisher_;
}; // VoidFutureNodeLogic

template <typename Result>
using VoidFutureNodeLogicPtr = std::shared_ptr<VoidFutureNodeLogic<Result>>;

/*!
 * Аналог FutureNode, но не требует спецификации входящего параметра
 * и поэтому позволяет подписаться на публикации разных типов.
 * Применение:
 * - некая логика которая должна мониторить другие разнородные узлы.
 * - первый узел в ExecutionTree которому для запуска не нужен входящий параметр.
 */
template <typename Result>
class VoidFutureNode : public INode
{
public:
	VoidFutureNode(
		std::weak_ptr<VoidFutureNode<Result>> self_weak,
		VoidFutureNodeLogicPtr<Result> future_node_logic,
		IHighWayMailBoxPtr highway_mailbox,
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		std::uint32_t node_id = 0)
		: INode(std::move(current_executed_node_publisher), node_id)
		, self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, highway_mailbox_{std::move(highway_mailbox)}
	{
	}

	template <typename R, typename P>
	static std::shared_ptr<VoidFutureNode<Result>> create(
		R && callback,
		P protector,
		std::shared_ptr<IHighWay> highway, //именно хайвей чтобы понять нужен ли ManyToMany паблишер
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

		auto future_node_logic = VoidFutureNodeLogic<Result>::create(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line,
			std::move(publisher));

		return make_self_shared<VoidFutureNode<Result>>(
			std::move(future_node_logic),
			highway->mailbox(),
			std::move(current_executed_node_publisher),
			node_id);
	} // create

	template <typename Parameter>
	Subscription<Parameter> subscription(bool send_may_fail = true) const
	{
		return send_may_fail ? subscription_send_may_fail<Parameter>() : subscription_send_may_blocked<Parameter>();
	}

	void execute() const
	{
		subscription<bool>().send(true);
	}

	template <typename Parameter>
	Subscription<Parameter> subscription_send_may_fail() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<VoidFutureNode<Result>> inode_weak,
				VoidFutureNodeLogicPtr<Result> future_node_logic,
				IHighWayMailBoxPtr highway_mailbox)
				: inode_weak_{std::move(inode_weak)}
				, future_node_logic_{std::move(future_node_logic)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Parameter) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				if (auto lock = inode_weak_.lock())
				{
					auto message = hi::Runnable::create(
						[this, inode_weak = inode_weak_, subscription_callback = future_node_logic_](
							const std::atomic<std::uint32_t> & global_run_id,
							const std::uint32_t your_run_id) mutable
						{
							if (auto inode = inode_weak.lock())
							{
								subscription_callback->send(*inode, global_run_id, your_run_id);
							}
						},
						future_node_logic_->get_code_filename(),
						future_node_logic_->get_code_line());
					return highway_mailbox_->send_may_fail(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<VoidFutureNode<Result>> inode_weak_;
			const VoidFutureNodeLogicPtr<Result> future_node_logic_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{self_weak_, future_node_logic_, highway_mailbox_}};
	}

	template <typename Parameter>
	Subscription<Parameter> subscription_send_may_blocked() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<VoidFutureNode<Result>> inode_weak,
				VoidFutureNodeLogicPtr<Result> future_node_logic,
				IHighWayMailBoxPtr highway_mailbox)
				: inode_weak_{std::move(inode_weak)}
				, future_node_logic_{std::move(future_node_logic)}
				, highway_mailbox_{std::move(highway_mailbox)}
			{
			}

			bool operator()(Parameter) override
			{
				if (!future_node_logic_->alive())
				{
					return false;
				}

				if (auto lock = inode_weak_.lock())
				{
					auto message = hi::Runnable::create(
						[this, inode_weak = inode_weak_, subscription_callback = future_node_logic_](
							const std::atomic<std::uint32_t> & global_run_id,
							const std::uint32_t your_run_id) mutable
						{
							if (auto inode = inode_weak.lock())
							{
								subscription_callback->send(*inode, global_run_id, your_run_id);
							}
						},
						future_node_logic_->get_code_filename(),
						future_node_logic_->get_code_line());
					return highway_mailbox_->send_may_blocked(std::move(message));
				}
				return false;
			}

			const std::weak_ptr<VoidFutureNode<Result>> inode_weak_;
			const VoidFutureNodeLogicPtr<Result> future_node_logic_;
			const IHighWayMailBoxPtr highway_mailbox_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{self_weak_, future_node_logic_, highway_mailbox_}};
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<VoidFutureNode<Result>> self_weak_;
	VoidFutureNodeLogicPtr<Result> future_node_logic_;
	IHighWayMailBoxPtr highway_mailbox_;
}; // VoidFutureNode

} // namespace hi

#endif // VoidFutureNode_H
