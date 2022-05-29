/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef INODE_H
#define INODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/execution_tree/CurrentExecutedNode.h>

#include <cstdint>
#include <memory>

namespace hi
{

/**
 * @brief The INode class
 * Base class for execution tree nodes.
 * This interface can be accessed from callbacks.
 * @note thread safe
 */
class INode
{
public:
	INode(IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher, std::uint32_t node_id)
		: current_executed_node_publisher_{std::move(current_executed_node_publisher)}
		, node_id_{node_id}
	{
	}

	virtual ~INode() = default;

	std::uint32_t node_id() const noexcept
	{
		return node_id_;
	}

	void publish_progress_state(bool in_progress, std::uint16_t achieved_progress)
	{
		if (current_executed_node_publisher_)
		{
			current_executed_node_publisher_->publish(CurrentExecutedNode{node_id_, achieved_progress, in_progress});
		}
	}

protected:
	const IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher_;
	const std::uint32_t node_id_;
};

using INodePtr = std::shared_ptr<INode>;

/*
  void publish_progress_state(bool in_progress, std::uint16_t achieved_progress)
*/
using PublishProgressStateCallback = std::function<void(bool, std::uint16_t)>;

} // namespace hi

#endif // INODE_H
