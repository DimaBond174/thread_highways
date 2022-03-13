#ifndef FutureNode_H
#define FutureNode_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/channels/PublishOneForMany.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

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
		unsigned int line)
	{
		struct FutureNodeLogicProtectedHolderImpl : public FutureNodeLogicHolder
		{
			FutureNodeLogicProtectedHolderImpl(R && callback, P && protector)
				: callback_{std::move(callback)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(Parameter publication, IPublisher<Result> & result_publisher) override
			{
				safe_invoke_void(callback_, protector_, std::move(publication), result_publisher);
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
			line);
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
		subscription_callback_holder_ = rhs.subscription_callback_holder_;
		future_result_publisher_ = rhs.future_result_publisher_;
		rhs.subscription_callback_holder_ = nullptr;
		return *this;
	}

	void send(Parameter publication) const
	{
		(*subscription_callback_holder_)(std::move(publication), *future_result_publisher_);
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

	struct FutureNodeLogicHolder
	{
		virtual ~FutureNodeLogicHolder() = default;
		virtual void operator()(Parameter publication, IPublisher<Result> & result_publisher) = 0;
		[[nodiscard]] virtual bool alive() = 0;
	};

	FutureNodeLogic(FutureNodeLogicHolder * subscription_holder, std::string filename, unsigned int line)
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
	FutureNodeLogicHolder * subscription_callback_holder_;
	std::shared_ptr<PublishOneForMany<Result>> future_result_publisher_;
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
		IHighWayMailBoxPtr high_way_mail_box)
		: self_weak_{std::move(self_weak)}
		, future_node_logic_{std::move(future_node_logic)}
		, high_way_mail_box_{std::move(high_way_mail_box)}
	{
	}

	FutureNode(
		std::weak_ptr<FutureNode<Parameter, Result>> self_weak,
		FutureNodeLogicPtr<Parameter, Result> future_node_logic,
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
	Subscription<Parameter> subscription()
	{
		struct SubscriptionProtectedHolderImpl : public Subscription<Parameter>::SubscriptionHolder
		{
			SubscriptionProtectedHolderImpl(
				FutureNodeLogicPtr<Parameter, Result> future_node_logic,
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
				if constexpr (send_may_fail)
				{
					return high_way_mail_box_->send_may_fail(std::move(message));
				}
				else
				{
					return high_way_mail_box_->send_may_blocked(std::move(message));
				}
			}

			FutureNodeLogicPtr<Parameter, Result> future_node_logic_;
			IHighWayMailBoxPtr high_way_mail_box_;
		};

		return Subscription<Parameter>{new SubscriptionProtectedHolderImpl{future_node_logic_, high_way_mail_box_}};
	}

	ISubscribeHerePtr<Result> result_channel()
	{
		return future_node_logic_->subscribe_result_channel();
	}

private:
	const std::weak_ptr<FutureNode<Parameter, Result>> self_weak_;
	FutureNodeLogicPtr<Parameter, Result> future_node_logic_;
	IHighWayMailBoxPtr high_way_mail_box_;
}; // FutureNode

} // namespace hi

#endif // FutureNode_H
