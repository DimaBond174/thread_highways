/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_IEXECUTIONTREE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_IEXECUTIONTREE_H

#include <thread_highways/execution_tree/DefaultNode.h>
#include <thread_highways/execution_tree/NodeProgress.h>
#include <thread_highways/execution_tree/ResultNode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <deque>

namespace hi
{

class IExecutionTree
{
public:
    virtual ~IExecutionTree() = default;
    virtual std::int32_t generate_node_id() = 0;
    virtual HighWayWeakPtr highway() = 0;
    virtual void add_node(std::shared_ptr<INode> node) = 0;
    virtual void remove_node(std::int32_t node_id) = 0;
    virtual void remove_all() = 0;
    virtual void execute() = 0;
    virtual void execute(std::int32_t node_id) = 0;
};

template <typename InParam, typename OutResult>
struct ExecutionTreeDefaultNodeFabric
{
    template<typename NodeLogic>
    static std::shared_ptr<DefaultNode<InParam, OutResult, NodeLogic>> create(IExecutionTree &tree, HighWayWeakPtr highway = nullptr, std::int32_t node_id = -1)
    {
        if (!highway)
        {
            highway = tree.highway();
        }
        if (-1 == node_id)
        {
            node_id = tree.generate_node_id();
        }
        auto node = make_self_shared<DefaultNode<InParam, OutResult, NodeLogic>>(std::move(highway), node_id);
        tree.add_node(node);
        return node;
    }

    template<typename NodeLogic>
    static std::shared_ptr<DefaultNode<InParam, OutResult, NodeLogic>> create(NodeLogic&& node_logic, IExecutionTree &tree, HighWayWeakPtr highway = nullptr, std::int32_t node_id = -1)
    {
        if (!highway)
        {
            highway = tree.highway();
        }
        if (-1 == node_id)
        {
            node_id = tree.generate_node_id();
        }
        auto node = make_self_shared<DefaultNode<InParam, OutResult, NodeLogic>>(std::move(node_logic), std::move(highway), node_id);
        tree.add_node(node);
        return node;
    }
}; // ExecutionTreeDefaultNodeFabric

template <typename InParam>
struct ExecutionTreeResultNodeFabric
{

    template<typename NodeLogic = NoLogic<InParam>>
    static std::shared_ptr<ResultNode<InParam, NodeLogic>> create(IExecutionTree &tree, HighWayWeakPtr highway = nullptr, std::int32_t node_id = -1)
    {
        if (!highway)
        {
            highway = tree.highway();
        }
        if (-1 == node_id)
        {
            node_id = tree.generate_node_id();
        }
        auto node = make_self_shared<ResultNode<InParam, NodeLogic>>(std::move(highway), node_id);
        tree.add_node(node);
        return node;
    }

    template<typename NodeLogic>
    static std::shared_ptr<ResultNode<InParam, NodeLogic>> create(NodeLogic&& node_logic, IExecutionTree &tree, HighWayWeakPtr highway = nullptr, std::int32_t node_id = -1)
    {
        if (!highway)
        {
            highway = tree.highway();
        }
        if (-1 == node_id)
        {
            node_id = tree.generate_node_id();
        }
        auto node = make_self_shared<ResultNode<InParam, NodeLogic>>(std::move(node_logic), std::move(highway), node_id);
        tree.add_node(node);
        return node;
    }
}; // ExecutionTreeDefaultNodeFabric

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_IEXECUTIONTREE_H
