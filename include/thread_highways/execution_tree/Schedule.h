/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_SCHEDULE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_SCHEDULE_H

#include <chrono>

namespace hi {

// Расписание запуска
struct Schedule
{
    /**
     * Auxiliary method for setting launch after a specified period
     *
     * @param ms - will start after now() + ms milliseconds
     */
    void schedule_launch_in(const std::chrono::milliseconds ms)
    {
            rechedule_ = true;
            next_execution_time_ = std::chrono::steady_clock::now() + ms;
    }

    // true == should be rescheduled. Automatically set to false before each run
    bool rechedule_{false};

    // Point in time after which the task should be launched for execution
    std::chrono::steady_clock::time_point next_execution_time_{}; // == run if less then now()
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_SCHEDULE_H
