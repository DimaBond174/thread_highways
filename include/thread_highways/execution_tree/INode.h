#ifndef INODE_H
#define INODE_H

#include <thread_highways/channels/IPublishSubscribe.h>
#include <thread_highways/execution_tree/CurrentExecutedNode.h>

#include <atomic>
#include <memory>

namespace hi
{

class INode
{
public:
	INode(
		IPublisherPtr<CurrentExecutedNode> current_executed_node_publisher = nullptr,
		std::uint32_t node_id = generate_node_id())
		: current_executed_node_publisher_{std::move(current_executed_node_publisher)}
		, node_id_{node_id}
	{
	}

	virtual ~INode() = default;

	std::uint32_t get_node_id() const noexcept
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
	static std::uint32_t generate_node_id()
	{
		static std::atomic<std::uint32_t> last_node_id{0};
		return last_node_id.fetch_add(1, std::memory_order_relaxed);
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
