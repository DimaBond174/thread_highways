/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef CURRENTEXECUTIONNODE_H
#define CURRENTEXECUTIONNODE_H

#include <cstdint>

namespace hi
{

/*
 Структура для визуализации процесса выполнения ExecutionTree
 каждый узел может информировать когда он начал работать и когда закончил,
 а также какого прогресса достиг
*/
struct CurrentExecutedNode
{
	std::uint32_t node_id_;
	std::uint16_t achieved_progress_;
	bool in_progress_;
};

} // namespace hi

#endif // CURRENTEXECUTIONNODE_H
