/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_RESULT_NODE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_RESULT_NODE_H

#include <thread_highways/execution_tree/i_node.h>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>

namespace hi
{

template <typename InParam>
struct NoLogic
{
	void operator()(hi::LabeledPublication<InParam>)
	{
	}
};

template <typename InParam, typename NodeLogic>
class ResultNode
	: public INode
	, public InParamChannel<InParam>
{
public:
	ResultNode(std::weak_ptr<ResultNode> self_weak, HighWayProxyPtr highway, std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
	{
	}

	ResultNode(std::weak_ptr<ResultNode> self_weak, std::shared_ptr<HighWay> highway, std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
	{
	}

	ResultNode(
		std::weak_ptr<ResultNode> self_weak,
		NodeLogic && node_logic,
		HighWayProxyPtr highway,
		std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
		, node_logic_{std::move(node_logic)}
	{
	}

	ResultNode(
		std::weak_ptr<ResultNode> self_weak,
		NodeLogic && node_logic,
		std::shared_ptr<HighWay> highway,
		std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
		, node_logic_{std::move(node_logic)}
	{
	}

public: // InParamChannel
	std::weak_ptr<ISubscription<InParam>> in_param_highway_channel(
		const std::int32_t label = {},
		bool send_may_fail = true) override
	{
		auto subscription = create_subscription<InParam>(
			[this, label](InParam publication, const std::atomic<bool> & keep_execution)
			{
				if (!std::is_same<NodeLogic, NoLogic<InParam>>::value)
				{
					node_logic_on_copy(publication, label, keep_execution);
				}
				set_result(std::move(LabeledPublication<InParam>{std::move(publication), label}));
				publish_progress_state(NodeProgress::ProgressSuccessFinished);
			},
			highway_,
			__FILE__,
			__LINE__,
			send_may_fail);

		execute(
			[&, label, subscription]() mutable
			{
				my_in_channels_.push(new LabeledStrongSubscription<InParam>{std::move(subscription), label});
			},
			__FILE__,
			__LINE__);

		return subscription;
	}

	// Подписка в потоке Паблишера, нет копирования публикации
	std::weak_ptr<ISubscription<InParam>> in_param_direct_channel(const std::int32_t label = {}) override
	{
		auto subscription = create_subscription<InParam>(
			[this, label](const InParam & publication)
			{
				if (!std::is_same<NodeLogic, NoLogic<InParam>>::value)
				{
					node_logic_on_copy(publication, label);
				}
				set_result(std::move(LabeledPublication<InParam>{publication, label}));
				publish_progress_state(NodeProgress::ProgressSuccessFinished);
			});

		my_in_channels_.push(new LabeledStrongSubscription<InParam>{subscription, label});

		return subscription;
	}

	void send_param_in_highway_channel(InParam param) override
	{
		auto in = in_param_highway_channel(-1, false);
		if (auto in_locked = in.lock())
		{
			in_locked->send(std::move(param));
		}
		else
		{
			assert(false);
		}

		delete_in_channels_on_highway(-1);
	}

	void send_param_in_direct_channel(const InParam & param) override
	{
		auto in = in_param_direct_channel(-1);
		if (auto in_locked = in.lock())
		{
			in_locked->send(param);
		}
		else
		{
			assert(false);
		}

		delete_in_channels_direct(-1);
	}

public:
	bool result_ready()
	{
		bool re = false;
		if (mutex_.try_lock())
		{
			re = !!result_;
			mutex_.unlock();
		}
		return re;
	}

	std::shared_ptr<LabeledPublication<InParam>> get_result()
	{
		std::unique_lock<std::mutex> lk{mutex_};
		cv_.wait(
			lk,
			[this]
			{
				return !!result_;
			});
		return result_;
	}

	void reset_result()
	{
		std::lock_guard<std::mutex> lk{mutex_};
		result_.reset();
	}

	// Внимание!: использовать этот метод с пониманием многопоточной обработки NodeLogic
	std::optional<NodeLogic> & node_logic()
	{
		return node_logic_;
	}

	// Например для суммирования произвольного x1 + x2 + ... xN
	// можно сделать так что новые x[N + 1] будут добавляться к сумме,
	// а старые x1, x2, x3 заменяться в сумме - и это будет дёшево так как не надо будте пересчитывать всю сумму
	// Например для нейросетей с динамически изменяющемся выводе на основе входящих данных
	// когда анализируются только те входящие пареметры которые изменились (а не все на каждой итерации)
	// Это генератор GUID для других узлов, а не инфа об этом узле.
	std::int32_t get_unique_label()
	{
		return last_unique_label_.fetch_add(1, std::memory_order_relaxed);
	}

protected: // INode
	// Удалить свои входящие каналы с меткой label
	void delete_in_channels_impl(const std::int32_t label) override
	{
		SingleThreadStack<LabeledStrongSubscription<InParam>> my_in_channels;
		while (auto holder = my_in_channels_.pop())
		{
			if (label == holder->label_)
			{
				delete holder;
			}
			else
			{
				my_in_channels.push(holder);
			}
		}
		my_in_channels_.swap(my_in_channels);
	}

	// Удалить все свои входящие каналы
	void delete_all_in_channels_impl() override
	{
		my_in_channels_.clear();
	}

	void delete_out_channels_impl(std::int32_t) override
	{
	}

	void delete_all_out_channels_impl() override
	{
	}

protected:
	void node_logic_on_copy(const InParam & param, const std::int32_t label, const std::atomic<bool> & keep_execution)
	{
		if constexpr (std::is_invocable_v<NodeLogic, const InParam &, const std::int32_t, const std::atomic<bool> &>)
		{
			node_logic_(param, label, keep_execution);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, const InParam &, const std::int32_t>)
		{
			node_logic_(param, label);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, const InParam &>)
		{
			node_logic_(param);
		}
		else if constexpr (can_be_dereferenced<NodeLogic &>::value)
		{
			auto && r = *node_logic_;
			if constexpr (
				std::is_invocable_v<decltype(r), const InParam &, const std::int32_t, const std::atomic<bool> &>)
			{
				r(param, label, keep_execution);
			}
			else if constexpr (std::is_invocable_v<decltype(r), const InParam &, const std::int32_t>)
			{
				r(param, label);
			}
			else
			{
				r(param);
			}
		}
		else
		{
			// The callback signature must be one of the above
			// ShowType<NodeLogic> xType;
			assert(false);
		}
	}

	void node_logic_on_copy(const InParam & param, const std::int32_t label)
	{
		if constexpr (std::is_invocable_v<NodeLogic, const InParam &, const std::int32_t>)
		{
			node_logic_(param, label);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, const InParam &>)
		{
			node_logic_(param);
		}
		else if constexpr (can_be_dereferenced<NodeLogic &>::value)
		{
			auto && r = *node_logic_;
			if constexpr (std::is_invocable_v<decltype(r), const InParam &, const std::int32_t>)
			{
				r(param, label);
			}
			else if constexpr (std::is_invocable_v<decltype(r), const InParam &>)
			{
				r(param);
			}
			else
			{
				// The callback signature must be one of the above
				// ShowType<NodeLogic> xType;
				assert(false);
			}
		}
		else
		{
			// The callback signature must be one of the above
			// ShowType<NodeLogic> xType;
			assert(false);
		}
	}

	void set_result(LabeledPublication<InParam> result)
	{
		{
			std::lock_guard<std::mutex> lk{mutex_};
			result_ = std::make_shared<LabeledPublication<InParam>>(std::move(result));
		}
		cv_.notify_one();
	}

protected:
	SingleThreadStack<LabeledStrongSubscription<InParam>> my_in_channels_;
	NodeLogic node_logic_;
	std::atomic<std::int32_t> last_unique_label_{0};

	std::mutex mutex_;
	std::condition_variable cv_;
	std::shared_ptr<LabeledPublication<InParam>> result_;
	std::shared_ptr<bool> protector_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_RESULT_NODE_H
