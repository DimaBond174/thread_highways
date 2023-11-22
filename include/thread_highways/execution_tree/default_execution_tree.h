/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULTEXECUTIONTREE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULTEXECUTIONTREE_H

#include <thread_highways/channels/highway_sticky_publisher.h>
#include <thread_highways/execution_tree/node_progress.h>
#include <thread_highways/execution_tree/i_execution_tree.h>

#include <deque>
#include <memory>

namespace hi
{

class DefaultExecutionTree : public IExecutionTree
{
public:
	DefaultExecutionTree(HighWayProxyPtr highway)
		: highway_{std::move(highway)}
	{
	}

	DefaultExecutionTree(std::shared_ptr<HighWay> highway)
		: highway_{make_proxy(std::move(highway))}
	{
	}

	// Запуск блок схемы
	template <typename InParam>
	bool start(InParam in_param, const std::int32_t node_id = {})
	{
		for (auto & it : tree_)
		{
			if (it->node_id() == node_id)
			{
				auto node = std::dynamic_pointer_cast<InParamChannel<InParam>>(it);
				if (node)
				{
					node->send_param_in_highway_channel(std::move(in_param));
					return true;
				}
			}
		}
		return false;
	}

public: // IExecutionTree
	std::int32_t generate_node_id() override
	{
		return last_id_.fetch_add(1, std::memory_order_relaxed);
	}

	HighWayProxyPtr highway() override
	{
		return highway_;
	}

	void add_node(std::shared_ptr<INode> node) override
	{
		if (!node)
			return;
		tree_.push_back(std::move(node));
	}

	std::shared_ptr<INode> get_node(std::int32_t node_id) override
	{
		for (auto & it : tree_)
		{
			if (it->node_id() == node_id)
			{
				return it;
			}
		}
		return {};
	}

	void remove_node(const std::int32_t node_id) override
	{
		for (auto it = tree_.begin(); it != tree_.end();)
		{
			if (it->get()->node_id() == node_id)
			{
				it = tree_.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void remove_all() override
	{
		tree_.clear();
	}

protected:
	HighWayProxyPtr highway_;
	std::atomic<std::int32_t> last_id_{0};
	std::deque<INodePtr> tree_;
};

class ExecutionTreeWithProgressPublisher : public DefaultExecutionTree
{
public:
	ExecutionTreeWithProgressPublisher(
		HighWayProxyPtr highway,
		std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher = nullptr)
		: DefaultExecutionTree{std::move(highway)}
		, progress_publisher_{
			  progress_publisher ? std::move(progress_publisher)
								 : make_self_shared<HighWayStickyPublisher<NodeProgress>>(highway_)}
	{
	}

	ExecutionTreeWithProgressPublisher(
		std::shared_ptr<HighWay> highway,
		std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher = nullptr)
		: DefaultExecutionTree{std::move(highway)}
		, progress_publisher_{
			  progress_publisher ? std::move(progress_publisher)
								 : make_self_shared<HighWayStickyPublisher<NodeProgress>>(highway_)}
	{
	}

	ISubscribeHerePtr<NodeProgress> subscribe_channel()
	{
		return progress_publisher_->subscribe_channel();
	}

public: // IExecutionTree
	void add_node(std::shared_ptr<INode> node) override
	{
		node->set_progress_publisher(progress_publisher_);
		DefaultExecutionTree::add_node(std::move(node));
	}

private:
	const std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_DEFAULTEXECUTIONTREE_H
