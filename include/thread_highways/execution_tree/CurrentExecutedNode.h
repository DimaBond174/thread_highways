#ifndef CURRENTEXECUTIONNODE_H
#define CURRENTEXECUTIONNODE_H

#include <cstdint>

namespace hi
{

/*
 Для визуализации процесса выполнения ExecutionTree
 каждый узел информирует когда он начал работать и когда закончил.
*/
struct CurrentExecutedNode
{
	std::uint32_t node_id_;
	std::uint16_t achieved_progress_;
	bool in_progress_;
};

} // namespace hi

#endif // CURRENTEXECUTIONNODE_H
