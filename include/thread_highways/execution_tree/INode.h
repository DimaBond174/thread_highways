/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H

#include <thread_highways/channels/HighWayStickyPublisher.h>
#include <thread_highways/channels/ISubscription.h>
#include <thread_highways/execution_tree/NodeProgress.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace hi
{

// Входящие параметры и исходящие результаты промечаются label
// Например в логическом ветвлении результата на if(true == label{1}) или else(false == label{0})
// Например входящие операнды арифметического выражения x[label{0}] + x[label{1}] / x[label{3}]
// Метка label позволяет смаршрутизировать входящий параметр и исходящий результат по блок схеме алгоритма
template<typename Publication>
struct LabeledPublication
{
    LabeledPublication(Publication publication, std::int32_t label = {})
        : publication_{std::move(publication)}
        , label_{label} {}

    Publication publication_;
    std::int32_t label_;
};

template<typename Publication>
struct LabeledWeakSubscription
{
    LabeledWeakSubscription(std::weak_ptr<ISubscription<Publication>> subscription, std::int32_t label = {})
        : subscription_{std::move(subscription)}
        , label_{label} {}

    const std::weak_ptr<ISubscription<Publication>> subscription_;
    const std::int32_t label_;
    LabeledWeakSubscription* next_in_stack_{nullptr};
};

template<typename Publication>
struct LabeledStrongSubscription
{
    LabeledStrongSubscription(std::shared_ptr<ISubscription<Publication>> subscription, std::int32_t label = {})
        : subscription_{std::move(subscription)}
        , label_{label} {}

    const std::shared_ptr<ISubscription<Publication>> subscription_;
    const std::int32_t label_;
    LabeledStrongSubscription* next_in_stack_{nullptr};
};

template<typename InParam>
class InParamChannel
{
public:
    // Получить входящий канал на определённый операнад
    // Например: (x1 + x2 * x3) / x4
    // - можно получить канал на каждый x[N] и слать на вход операции операнд сразу в нужное место
    // Важно: подписка работает пока хранится shared_ptr, поэтому
    // std::shared_ptr<ISubscription<LabeledVar<T>>> subscription необходимо хранить у себя
    // (например в std::map<std::int32_t, shared_> или иным способом если например label{x} используется
    // для подписки на результаты нескольких источников
    virtual std::weak_ptr<ISubscription<InParam>> in_param_channel(std::int32_t  label = {}, bool send_may_fail = true) = 0;
};

template<typename OutResult>
class OutResultChannel
{
public:
    // Подписка на определённый результат
    // Например в узле логический if(true) else (false) == можно подписаться на label(0) и label(1) соответственно false/true
    // Можно организовать сразу более сложную логику с логическим ветвлением на std::int32_t вариантов (не только false/true)
    virtual void subscribe(std::weak_ptr<ISubscription<OutResult>>, std::int32_t  label = {}) = 0;
};

/**
 * @brief The INode class
 * Base class for execution tree nodes.
 * This interface can be accessed from callbacks.
 * @note thread safe
 */
class INode
{
public:
    INode(std::weak_ptr<INode> self_weak, HighWayWeakPtr highway,  std::int32_t node_id)
                : self_weak_{std::move(self_weak)}
                , highway_{std::move(highway)}
                , node_id_{node_id}
        {
        }

    INode(std::weak_ptr<INode> self_weak, std::shared_ptr<HighWay> highway,  std::int32_t node_id)
                : self_weak_{std::move(self_weak)}
                , highway_{hi::make_weak_ptr(std::move(highway))}
                , node_id_{node_id}
        {
        }

	virtual ~INode() = default;

    void set_progress_publisher(std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher)
    {
        progress_publisher_ = std::move(progress_publisher);
    }

    std::int32_t node_id() noexcept
	{
		return node_id_;
	}   

    void publish_progress_state(std::int32_t achieved_progress)
	{
        if (progress_publisher_)
		{
            progress_publisher_->publish(NodeProgress{node_id_, achieved_progress});
		}
	}

    // Удалить свои входящие каналы с меткой label
    void delete_in_channels(std::int32_t label)
    {
        execute([this, label]  { delete_in_channels_impl(label);  }, __FILE__, __LINE__);
    }

    // Удалить все свои входящие каналы
    virtual void delete_all_in_channels()
    {
        execute([this]  { delete_all_in_channels_impl();  }, __FILE__, __LINE__);
    }

    // Удалить свои исходящие каналы с меткой label
    void delete_out_channels(std::int32_t label)
    {
        execute([this, label]  { delete_out_channels(label);  }, __FILE__, __LINE__);
    }

    // Удалить все свои исходящие каналы
    void delete_all_out_channels()
    {
        execute([this]  { delete_all_out_channels();  }, __FILE__, __LINE__);
    }

    // Подключение результата этого узла на вход следующего узла
    template<typename OutResult>
    bool connect_to(INode *next_node, const std::int32_t  result_label, const std::int32_t  label_in_next, bool send_may_fail = true)
    {
        if (!next_node) return false;
        InParamChannel<OutResult> *next_node_in = dynamic_cast<InParamChannel<OutResult>*>(next_node);
        if (!next_node_in) return false;
        OutResultChannel<OutResult> *result = dynamic_cast<OutResultChannel<OutResult>*>(this);
        if (!result) return false;
        return result->subscribe(next_node_in->in_param_channel(label_in_next, send_may_fail), result_label);
    }

   virtual void execute() = 0;

protected:
    // Запустить на highway_ под защитой self_weak_
    template<typename R>
    void execute(R&& r, const char* filename, unsigned int line)
    {
        highway_->execute(Runnable::create(std::move(r), self_weak_, filename, line));
    }

    // Удалить свои входящие каналы с меткой label
    virtual void delete_in_channels_impl(std::int32_t label) = 0;

    // Удалить все свои входящие каналы
    virtual void delete_all_in_channels_impl() = 0;

    // Удалить свои исходящие каналы с меткой label
    virtual void delete_out_channels_impl(std::int32_t label) = 0;

    // Удалить все свои исходящие каналы
    virtual void delete_all_out_channels_impl() = 0;

protected:
    const std::weak_ptr<INode> self_weak_;
    const HighWayWeakPtr highway_;
    const std::int32_t node_id_;

    std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher_;
};

using INodePtr = std::shared_ptr<INode>;

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H
