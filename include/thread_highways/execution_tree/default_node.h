/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H

#include <thread_highways/execution_tree/i_node.h>

#include <optional>

namespace hi
{

/*
	NodeLogic - класс функтора с сигнатурой
	std::optional<LabeledPublication<OutResult>>(LabeledPublication<InParam>, INode&, const std::atomic<bool>&
   keep_execution) Можно передать экземпляр. Если экземпляр NodeLogic не передан в конструкторе, то будет Default.
*/
template <typename InParam, typename OutResult, typename NodeLogic>
class DefaultNode
	: public INode
	, public InParamChannel<InParam>
	, public Publisher<OutResult>
{
public:
	DefaultNode(std::weak_ptr<DefaultNode> self_weak, HighWayProxyPtr highway, std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
	{
	}

	DefaultNode(std::weak_ptr<DefaultNode> self_weak, std::shared_ptr<HighWay> highway, std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
	{
	}

	DefaultNode(
		std::weak_ptr<DefaultNode> self_weak,
		NodeLogic && node_logic,
		HighWayProxyPtr highway,
		std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
		, node_logic_{std::move(node_logic)}
	{
	}

	DefaultNode(
		std::weak_ptr<DefaultNode> self_weak,
		NodeLogic && node_logic,
		std::shared_ptr<HighWay> highway,
		std::int32_t node_id)
		: INode{std::move(self_weak), std::move(highway), node_id}
		, node_logic_{std::move(node_logic)}
	{
	}

public: // InParamChannel
	// Подписка с перепланированием на highway
	// label - какой меткой будут промечаться все входящие на этом канале
	std::weak_ptr<ISubscription<InParam>> in_param_highway_channel(
		const std::int32_t label = {},
		bool send_may_fail = true) override
	{
		auto subscription = create_subscription<InParam>(
			[this, label](InParam publication, const std::atomic<bool> & keep_execution)
			{
				node_logic_on_move(std::move(publication), label, keep_execution);
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
	// label - какой меткой будут промечаться все входящие на этом канале
	std::weak_ptr<ISubscription<InParam>> in_param_direct_channel(const std::int32_t label = {}) override
	{
		auto subscription = create_subscription<InParam>(
			[this, label](const InParam & publication)
			{
				node_logic_on_copy(publication, label);
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

public: // Publisher
	// Прогресс или код ошибки
	void progress(std::int32_t achieved_progress) override
	{
		publish_progress_state(achieved_progress);
	}

	// Публикации
	void publish_on_highway(LabeledPublication<OutResult> publication) override
	{
		execute(
			[&, publication = std::move(publication)]() mutable
			{
				publish_result(std::move(publication));
			},
			__FILE__,
			__LINE__);
	}

	void publish_direct(LabeledPublication<OutResult> publication) override
	{
		publish_result(std::move(publication));
	}

	void subscribe_on_highway(std::weak_ptr<ISubscription<OutResult>> subscription, const std::int32_t label = {})
		override
	{
		execute(
			[&, label, subscription = std::move(subscription)]() mutable
			{
				subscriptions_for_my_results_.push(
					new LabeledWeakSubscription<OutResult>{std::move(subscription), label});
			},
			__FILE__,
			__LINE__);
	}

	void subscribe_direct(std::weak_ptr<ISubscription<OutResult>> subscription, const std::int32_t label = {}) override
	{
		subscriptions_for_my_results_.push(new LabeledWeakSubscription<OutResult>{std::move(subscription), label});
	}

public:
	// Внимание!: использовать этот метод с пониманием многопоточной обработки NodeLogic
	NodeLogic & node_logic()
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

	// Удалить свои исходящие каналы с меткой label
	void delete_out_channels_impl(std::int32_t label) override
	{
		SingleThreadStack<LabeledWeakSubscription<OutResult>> subscriptions;
		while (auto holder = subscriptions_for_my_results_.pop())
		{
			if (label == holder->label_)
			{
				delete holder;
			}
			else
			{
				subscriptions_for_my_results_.push(holder);
			}
		}
		subscriptions_for_my_results_.swap(subscriptions);
	}

	// Удалить все свои исходящие каналы
	void delete_all_out_channels_impl() override
	{
		subscriptions_for_my_results_.clear();
	}

protected:
	// Внимание!: использовать этот метод только из NodeLogic (который отрабатывает в потоке highway_)
	// или быть уверенным что не возникнет новых подписок из других потоков
	void publish_result(LabeledPublication<OutResult> result)
	{
		SingleThreadStack<LabeledWeakSubscription<OutResult>> subscriptions_for_my_results;
		while (auto holder = subscriptions_for_my_results_.pop())
		{
			if (result.label_ != holder->label_)
			{
				subscriptions_for_my_results.push(holder);
				continue;
			}

			if (auto subscriber = holder->subscription_.lock())
			{
				if (subscriber->send(result.publication_))
				{
					subscriptions_for_my_results.push(holder);
				}
				else
				{
					delete holder;
				}
			}
			else
			{
				delete holder;
			}
		}
		subscriptions_for_my_results_.swap(subscriptions_for_my_results);
	}

	void node_logic_on_move(InParam param, const std::int32_t label, const std::atomic<bool> & keep_execution)
	{
		if constexpr (std::is_invocable_v<
						  NodeLogic,
						  InParam,
						  const std::int32_t,
						  Publisher<OutResult> &,
						  const std::atomic<bool> &>)
		{
			node_logic_(std::move(param), label, *this, keep_execution);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, InParam, const std::int32_t, Publisher<OutResult> &>)
		{
			node_logic_(std::move(param), label, *this);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, InParam, const std::int32_t>)
		{
			node_logic_(std::move(param), label);
		}
		else if constexpr (std::is_invocable_v<NodeLogic, InParam>)
		{
			node_logic_(std::move(param));
		}
		else if constexpr (can_be_dereferenced<NodeLogic &>::value)
		{
			auto && r = *node_logic_;
			if constexpr (std::is_invocable_v<
							  decltype(r),
							  InParam,
							  const std::int32_t,
							  Publisher<OutResult> &,
							  const std::atomic<bool> &>)
			{
				r(std::move(param), label, *this, keep_execution);
			}
			else if constexpr (std::is_invocable_v<decltype(r), InParam, const std::int32_t, Publisher<OutResult> &>)
			{
				r(std::move(param), label, *this);
			}
			else if constexpr (std::is_invocable_v<decltype(r), InParam, const std::int32_t>)
			{
				r(std::move(param), label);
			}
			else
			{
				r(std::move(param));
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
		if constexpr (std::is_invocable_v<NodeLogic, const InParam &, const std::int32_t, Publisher<OutResult> &>)
		{
			node_logic_(param, label, *this);
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
			if constexpr (std::is_invocable_v<
							  decltype(r),
							  InParam,
							  const std::int32_t,
							  Publisher<OutResult> &,
							  const std::atomic<bool> &>)
			{
				static const std::atomic_bool keep_execution{true};
				r(std::move(param), label, *this, keep_execution);
			}
			else if constexpr (
				std::is_invocable_v<decltype(r), const InParam &, const std::int32_t, Publisher<OutResult> &>)
			{
				r(param, label, *this);
			}
			else if constexpr (std::is_invocable_v<decltype(r), const InParam &, const std::int32_t>)
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

protected:
	SingleThreadStack<LabeledStrongSubscription<InParam>> my_in_channels_;
	SingleThreadStack<LabeledWeakSubscription<OutResult>> subscriptions_for_my_results_;
	NodeLogic node_logic_;
	std::atomic<std::int32_t> last_unique_label_{0};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H
