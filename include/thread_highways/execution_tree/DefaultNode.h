/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H

#include <thread_highways/execution_tree/INode.h>

#include <optional>

namespace hi {

/*
    NodeLogic - класс функтора с сигнатурой
    std::optional<LabeledPublication<OutResult>>(LabeledPublication<InParam>, INode&, const std::atomic<bool>& keep_execution)
    Можно передать экземпляр. Если экземпляр NodeLogic не передан в конструкторе, то будет Default.
*/
template <typename InParam, typename OutResult, typename NodeLogic>
class DefaultNode : public INode, public InParamChannel<InParam>, public OutResultChannel<OutResult>
{
public:
    DefaultNode(std::weak_ptr<DefaultNode> self_weak, HighWayWeakPtr highway,  std::int32_t node_id)
                : INode{std::move(self_weak), std::move(highway) , node_id}
    {
    }

    DefaultNode(std::weak_ptr<DefaultNode> self_weak, std::shared_ptr<HighWay> highway,  std::int32_t node_id)
                : INode{std::move(self_weak), std::move(highway) , node_id}
    {
    }

    DefaultNode(std::weak_ptr<DefaultNode> self_weak, NodeLogic&& node_logic, HighWayWeakPtr highway, std::int32_t node_id)
                : INode{std::move(self_weak), std::move(highway) , node_id}
                , node_logic_{std::move(node_logic)}
    {
    }

    DefaultNode(std::weak_ptr<DefaultNode> self_weak, NodeLogic&& node_logic, std::shared_ptr<HighWay> highway, std::int32_t node_id)
                : INode{std::move(self_weak), std::move(highway) , node_id}
                , node_logic_{std::move(node_logic)}
    {
    }

public: // InParamChannel
    std::weak_ptr<ISubscription<InParam>> in_param_channel(const std::int32_t  label = {}, bool send_may_fail = true) override
    {
        auto subscription = create_subscription<InParam>([this, label](InParam publication, const std::atomic<bool>& keep_execution)
        {
            publish_progress_state(NodeProgress::ProgressStarted);
            std::optional<LabeledPublication<OutResult>> res = node_logic(LabeledPublication<InParam>{std::move(publication), label}, keep_execution);
            if (res)
            {
                publish_result(std::move(*res));
            }
            // Если есть желание оставить узел покрашенным зелёным на завершение
            // или красным на ошибку, то GUI может просто проигнорировать ProgressNotStarted состояние
            // Если есть необходимость доносить до GUI более развёрнутую ошибку, то можно воспользоваться hi::Exception или иным каналом
            publish_progress_state(NodeProgress::ProgressNotStarted);
         }, highway_, __FILE__, __LINE__, send_may_fail);

        highway_->execute([&, label, subscription]() mutable
        {
            my_in_channels_.push(new LabeledStrongSubscription<InParam>{std::move(subscription), label});
        });

        return subscription;
    }

public: // OutResultChannel
    void subscribe(std::weak_ptr<ISubscription<OutResult>> subscription, const std::int32_t  label = {}) override
    {
        highway_->execute([&, label, subscription = std::move(subscription)]() mutable
        {
            subscriptions_for_my_results_.push(new LabeledWeakSubscription<OutResult>{std::move(subscription), label});
        });
    }

public:
    void execute(InParam param)
    {
        auto in = in_param_channel(-1, false);
        if (auto in_locked = in.lock())
        {
            in_locked->send(std::move(param));
        } else {
            assert(false);
        }

        delete_in_channels(-1);
    }

    void execute() override
    {
        execute({});
    }

    // Внимание!: использовать этот метод с пониманием многопоточной обработки NodeLogic
    NodeLogic& node_logic()
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
        while(auto holder = my_in_channels_.pop())
        {
            if (label == holder->label_)
            {
                delete holder;
            } else {
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
        while(auto holder = subscriptions_for_my_results_.pop())
        {
            if (label == holder->label_)
            {
                delete holder;
            } else {
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
    void publish_result(LabeledPublication<OutResult> result)
    {
        SingleThreadStack<LabeledWeakSubscription<OutResult>> subscriptions_for_my_results;
        while(auto holder = subscriptions_for_my_results_.pop())
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
                } else {
                    delete holder;
                }
            } else {
                delete holder;
            }
        }
        subscriptions_for_my_results_.swap(subscriptions_for_my_results);
    }

    std::optional<LabeledPublication<OutResult>> node_logic(LabeledPublication<InParam> param,  const std::atomic<bool>& keep_execution)
    {
        if constexpr (std::is_invocable_v<NodeLogic, LabeledPublication<InParam>, INode&, const std::atomic<bool>& >)
        {
            return node_logic_(std::move(param), *this, keep_execution);
        }
        else if constexpr (std::is_invocable_v<NodeLogic, LabeledPublication<InParam>>)
        {
            return node_logic_(std::move(param));
        }
        else if constexpr (can_be_dereferenced<NodeLogic &>::value)
        {
            auto&& r = *node_logic_;
            if constexpr (std::is_invocable_v<decltype(r), LabeledPublication<InParam>, INode&, const std::atomic<bool>&  >)
            {
                return r(std::move(param), *this, keep_execution);
            }
            else
            {
                return r(std::move(param));
            }
        }
        else
        {
            // The callback signature must be one of the above
            // ShowType<NodeLogic> xType;
            assert(false);
        }
        return {};
    }

protected:
    SingleThreadStack<LabeledStrongSubscription<InParam>> my_in_channels_;
    SingleThreadStack<LabeledWeakSubscription<OutResult>> subscriptions_for_my_results_;
    NodeLogic node_logic_;
    std::atomic<std::int32_t> last_unique_label_{0};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULT_NODE_H
