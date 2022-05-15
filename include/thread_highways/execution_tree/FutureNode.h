/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef FutureNode_H
#define FutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishManyForMany.h>
#include <thread_highways/channels/PublishManyForManyCanUnSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <cassert>
#include <type_traits>

namespace hi
{

template <typename Parameter, typename Result>
class FutureNodeLogic
{
public:
	template <typename R, typename P>
	static std::shared_ptr<FutureNodeLogic<Parameter, Result>> create(
		R && callback,
		P protector,
		std::string filename,
		unsigned int line,
		std::shared_ptr<IPublisher<Result>> future_result_publisher)
	{
		struct FutureNodeLogicProtectedHolderImpl : public FutureNodeLogicHolder
		{
			FutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				[[maybe_unused]] Parameter publication,
				[[maybe_unused]] IPublisher<Result> & result_publisher,
				[[maybe_unused]] INode & node,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<
								  R,
								  Parameter,
								  IPublisher<Result> &,
								  INode &,
								  const std::atomic<std::uint32_t> &,
								  const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						result_publisher,
						node,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Parameter, IPublisher<Result> &, INode &>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication), result_publisher, node);
				}
				else if constexpr (std::is_invocable_v<
									   R,
									   Parameter,
									   IPublisher<Result> &,
									   const std::atomic<std::uint32_t> &,
									   const std::uint32_t>)
				{
					safe_invoke_void(
						callback_,
						protector_,
						std::move(publication),
						result_publisher,
						global_run_id,
						your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Parameter, IPublisher<Result> &>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication), result_publisher);
				}
				else if constexpr (std::is_invocable_v<R, Parameter>)
				{
					safe_invoke_void(callback_, protector_, std::move(publication));
				}
				else if constexpr (std::is_invocable_v<
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
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && callback = *callback_;
					if constexpr (std::is_invocable_v<
									  decltype(callback),
									  Parameter,
									  IPublisher<Result> &,
									  INode &,
									  const std::atomic<std::uint32_t> &,
									  const std::uint32_t>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(publication),
							result_publisher,
							node,
							global_run_id,
							your_run_id);
					}
					else if constexpr (std::
										   is_invocable_v<decltype(callback), Parameter, IPublisher<Result> &, INode &>)
					{
						safe_invoke_void(callback, protector_, std::move(publication), result_publisher, node);
					}
					else if constexpr (std::is_invocable_v<
										   decltype(callback),
										   Parameter,
										   IPublisher<Result> &,
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						safe_invoke_void(
							callback,
							protector_,
							std::move(publication),
							result_publisher,
							global_run_id,
							your_run_id);
					}
					else if constexpr (std::is_invocable_v<decltype(callback), Parameter, IPublisher<Result> &>)
					{
						safe_invoke_void(callback, protector_, std::move(publication), result_publisher);
					}
					else if constexpr (std::is_invocable_v<decltype(callback), Parameter>)
					{
						safe_invoke_void(callback, protector_, std::move(publication));
					}
					else if constexpr (std::is_invocable_v<
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
		return std::make_shared<FutureNodeLogic<Parameter, Result>>(
			new FutureNodeLogicProtectedHolderImpl{std::move(callback), std::move(protector)},
			std::move(filename),
			line,
			std::move(future_result_publisher));
	}

	~FutureNodeLogic()
	{
		delete subscription_callback_holder_;
	}
	FutureNodeLogic(const FutureNodeLogic & rhs) = delete;
	FutureNodeLogic & operator=(const FutureNodeLogic & rhs) = delete;
	FutureNodeLogic(FutureNodeLogic && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, subscription_callback_holder_{rhs.subscription_callback_holder_}
		, future_result_publisher_{rhs.future_result_publisher_}
	{
		rhs.subscription_callback_holder_ = nullptr;
	}
	FutureNodeLogic & operator=(FutureNodeLogic && rhs)
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
		Parameter publication,
		INode & node,
		const std::atomic<std::uint32_t> & global_run_id,
		const std::uint32_t your_run_id) const
	{
		(*subscription_callback_holder_)(
			std::move(publication),
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

	struct FutureNodeLogicHolder
	{
		virtual ~FutureNodeLogicHolder() = default;
		virtual void operator()(
			[[maybe_unused]] Parameter publication,
			IPublisher<Result> & result_publisher,
			[[maybe_unused]] INode & node,
			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
			[[maybe_unused]] const std::uint32_t your_run_id) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	FutureNodeLogic(
		FutureNodeLogicHolder * subscription_holder,
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
	FutureNodeLogicHolder * subscription_callback_holder_{nullptr};
	std::shared_ptr<IPublisher<Result>> future_result_publisher_;
}; // FutureNodeLogic

template <typename Parameter, typename Result>
using FutureNodeLogicPtr = std::shared_ptr<FutureNodeLogic<Parameter, Result>>;

template <typename Parameter, typename Result>
class FutureNode : public INode
{
public:
	FutureNode(
		std::weak_ptr<FutureNode<Parameter, Result>> self_weak,
		FutureNodeLogicPtr<Parameter, Result> future_node_logic,
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
	static std::shared_ptr<FutureNode<Parameter, Result>> create(
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

		auto future_node_logic = FutureNodeLogic<Parameter, Result>::create(
			std::move(callback),
			std::move(protector),
			std::move(filename),
			line,
			std::move(publisher));

		return make_self_shared<FutureNode<Parameter, Result>>(
			std::move(future_node_logic),
			highway->mailbox(),
			std::move(current_executed_node_publisher),
			node_id);
	} // create

	Subscription<Parameter> subscription(bool send_may_fail = true) const
	{
		return send_may_fail ? subscription_send_may_fail() : subscription_send_may_blocked();
	}

	void execute() const
	{
		subscription().send(Parameter{});
	}

	Subscription<Parameter> subscription_send_may_fail() const
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				std::weak_ptr<FutureNode<Parameter, Result>> inode_weak,
				FutureNodeLogicPtr<Parameter, Result> future_node_logic,
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

			const std::weak_ptr<FutureNode<Parameter, Result>> inode_weak_;
			const FutureNodeLogicPtr<Parameter, Result> future_node_logic_;
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
				std::weak_ptr<FutureNode<Parameter, Result>> inode_weak,
				FutureNodeLogicPtr<Parameter, Result> future_node_logic,
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

			const std::weak_ptr<FutureNode<Parameter, Result>> inode_weak_;
			FutureNodeLogicPtr<Parameter, Result> future_node_logic_;
			IHighWayMailBoxPtr highway_mailbox_;
		};

		return Subscription<Parameter>{
			new SubscriptionProtectedHolderImpl{self_weak_, future_node_logic_, highway_mailbox_}};
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<FutureNode<Parameter, Result>> self_weak_;
	const FutureNodeLogicPtr<Parameter, Result> future_node_logic_;
	const IHighWayMailBoxPtr highway_mailbox_;
}; // FutureNode

} // namespace hi

#endif // FutureNode_H
