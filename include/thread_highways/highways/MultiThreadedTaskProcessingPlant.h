/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_HIGHWAYS_MULTITHREADEDTASKPROCESSINGPLANT_H
#define THREADS_HIGHWAYS_HIGHWAYS_MULTITHREADEDTASKPROCESSINGPLANT_H

#include <thread_highways/execution_tree/Runnable.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/raii_thread.h>

#include <future>
#include <vector>

namespace hi {

// Цех многопоточной обработки
class MultiThreadedTaskProcessingPlant
{
public:
    MultiThreadedTaskProcessingPlant(std::weak_ptr<MultiThreadedTaskProcessingPlant> self_weak,
            std::uint32_t number_of_workers = 2,
            ExceptionHandler exception_handler = [](const hi::Exception& ex){ throw ex; },
            std::string name = "MultiThreadedTaskProcessingPlant",
            std::chrono::milliseconds max_task_execution_time = {},
            const std::uint32_t mail_box_capacity = 65000u)
    : exception_handler_{std::move(exception_handler)}
    , name_{std::move(name)}
    , max_task_execution_time_{max_task_execution_time}
    {
        set_capacity(mail_box_capacity);
        if (number_of_workers < 1) number_of_workers = 1;

        if (max_task_execution_time_ == std::chrono::milliseconds{})
        {
            start_workers_without_time_control(self_weak.lock(), number_of_workers);
        } else {
            start_workers_with_time_control(self_weak.lock(), number_of_workers);
        }
    }

    void set_capacity(const std::uint32_t capacity)
    {
        if (capacity)
        {
            mail_box_.set_capacity(capacity);
        }
    }

    // Будет пытаться добавить задачу, если ресурсов не осталось то заблокируется в ожидании
    void execute(Runnable&& runnable)
    {
        execute_impl(std::move(runnable));
    }

    /**
     * Setting a task for execution
     *
     * @param r - task for execution
     * @param send_may_fail - is this task required to be completed?
     *	(some tasks can be skipped if there is not enough RAM)
     * @param filename - file where the code is located
     * @param line - line in the file that contains the code
     */
    template <typename R>
    void execute(
            R&& r,
            const char* filename = __FILE__,
            const unsigned int line = __LINE__)
    {
            execute_impl(Runnable::create<R>(std::move(r), filename, line));
    }

    /**
     * Setting a task for execution
     *
     * @param r - task for execution
     * @param protector - task protection (a way to cancel task)
     * @param send_may_fail - is this task required to be completed?
     *	(some tasks can be skipped if there is not enough RAM)
     * @param filename - file where the code is located
     * @param line - line in the file that contains the code
     */
    template <typename R, typename P>
    void execute(
            R && r,
            P protector,
            const char* filename,
            const unsigned int line)
    {
            execute_impl(Runnable::create<R>(std::move(r), std::move(protector), filename, line));
    }

    // Попытается добавить задачу, если ресурсов не осталось, то вернёт false
    bool try_execute(Runnable && runnable)
    {
        return try_execute_impl(std::move(runnable));
    }

    /**
     * Setting a task for execution
     *
     * @param r - task for execution
     * @param send_may_fail - is this task required to be completed?
     *	(some tasks can be skipped if there is not enough RAM)
     * @param filename - file where the code is located
     * @param line - line in the file that contains the code
     */
    template <typename R>
    bool try_execute(
            R && r,
            const char* filename = __FILE__,
            const unsigned int line = __LINE__)
    {
            return try_execute_impl(Runnable::create<R>(std::move(r), filename, line));
    }

    /**
     * Setting a task for execution
     *
     * @param r - task for execution
     * @param protector - task protection (a way to cancel task)
     * @param send_may_fail - is this task required to be completed?
     *	(some tasks can be skipped if there is not enough RAM)
     * @param filename - file where the code is located
     * @param line - line in the file that contains the code
     */
    template <typename R, typename P>
    bool try_execute(
            R && r,
            P protector,
            const char* filename,
            const unsigned int line)
    {
            return try_execute_impl(Runnable::create<R>(std::move(r), std::move(protector), filename, line));
    }

    void destroy()
    {
        keep_execution_.store(false, std::memory_order_release);
        mail_box_.destroy();
        for (auto& it : workers_)
        {
            it.join();
        }
    }

private:
    void execute_impl(Runnable&& runnable)
    {
        mail_box_.send_may_blocked(std::move(runnable));
    }

    bool try_execute_impl(Runnable && runnable)
    {
        return mail_box_.send_may_fail(std::move(runnable));
    }

    void start_workers_without_time_control(const std::shared_ptr<MultiThreadedTaskProcessingPlant> self_protector, std::uint32_t number_of_workers)
    {
        for (std::uint32_t i = 0; i < number_of_workers; ++i)
        {
            workers_.emplace_back(RAIIthread(std::thread(
                                                 [this, self_protector]
                                                 {
                                                         worker_loop_without_time_control(self_protector);
                                                 })));
        }
    }

    void  worker_loop_without_time_control(const std::shared_ptr<MultiThreadedTaskProcessingPlant> self_protector)
    {
        const auto execute_runnable = [&](Holder<Runnable> * holder)
        {
            if (!holder) return;
            try
            {
                holder->t_.run(keep_execution_);
            }
            catch (const hi::Exception& e)
            {
                 exception_handler_(e);
            }
            catch (...)
            {
                exception_handler_(hi::Exception{name_ + ": ", __FILE__, __LINE__, std::current_exception()});
            }
            mail_box_.free_holder(holder);
        };

        // main loop
        while (keep_execution_.load(std::memory_order_relaxed))
        {
            execute_runnable(mail_box_.pop_message());
        } // while main loop

        self_protector->keep_execution_ = false;
    } // worker_loop_without_time_control

    void start_workers_with_time_control(const std::shared_ptr<MultiThreadedTaskProcessingPlant> self_protector, std::uint32_t number_of_workers)
    {
        for (std::uint32_t i = 0; i < number_of_workers; ++i)
        {
            workers_.emplace_back(RAIIthread(std::thread(
                                                 [this, self_protector]
                                                 {
                                                         worker_loop_with_time_control(self_protector);
                                                 })));
        }
    }

    void  worker_loop_with_time_control(const std::shared_ptr<MultiThreadedTaskProcessingPlant> self_protector)
    {
        auto before_time = std::chrono::steady_clock::now();
        const auto execute_runnable = [&](Holder<Runnable> * holder)
        {
            if (!holder) return;
            before_time = std::chrono::steady_clock::now();
            try
            {
                holder->t_.run(keep_execution_);
            }
            catch (const hi::Exception& e)
            {
                 exception_handler_(e);
            }
            catch (...)
            {
                exception_handler_(hi::Exception{name_ + ": ", __FILE__, __LINE__, std::current_exception()});
            }
            const auto after_time = std::chrono::steady_clock::now();
            const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
            before_time = after_time;
            if (time_diff > max_task_execution_time_)
            {
                exception_handler_(
                            hi::Exception{name_ + ":Runnable:stuck for ms = " + std::to_string(time_diff.count()),
                                          holder->t_.get_code_filename(), holder->t_.get_code_line()});
            }

            mail_box_.free_holder(holder);
        };

        // main loop
        while (keep_execution_.load(std::memory_order_relaxed))
        {
            execute_runnable(mail_box_.pop_message());
        } // while main loop

        self_protector->keep_execution_ = false;
    } // worker_loop_with_time_control

private:
    const ExceptionHandler exception_handler_;
    const std::string name_;
    const std::chrono::milliseconds max_task_execution_time_;

    std::vector<RAIIthread> workers_;
    MailBox<Runnable> mail_box_;

    // одноразовый рубильник
    std::atomic<bool> keep_execution_{true};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_HIGHWAYS_MULTITHREADEDTASKPROCESSINGPLANT_H
