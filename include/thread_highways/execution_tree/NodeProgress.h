/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_NODE_PROGRESS_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_NODE_PROGRESS_H

#include <cstdint>

namespace hi
{

/*
 Структура для визуализации процесса выполнения ExecutionTree
 каждый узел может информировать когда он начал работать и когда закончил,
 а также какого прогресса достиг
*/
struct NodeProgress
{    
    // идентификатор узла алгоритма
    std::int32_t node_id_;

    // 1 .. 10000 : прогресс (100% == 10000)
    // < 0 : код ошибки
    // 0 == не запущено
    // 10000 == успешно завершено
    // > 10000 == пользовательские коды завершения
    std::int32_t achieved_progress_{ProgressNotStarted};

    static const std::int32_t ProgressNotStarted{0};
    static const std::int32_t ProgressStarted{1};
    static const std::int32_t ProgressSuccessFinished{10000};
    static const std::int32_t ProgressUnknownError{-1};

    bool operator==(const NodeProgress& other)
    {
        return other.node_id_ == node_id_ && other.achieved_progress_ == achieved_progress_;
    }
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_NODE_PROGRESS_H
