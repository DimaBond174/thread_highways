/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_RESCHEDULABLERUNNABLE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_RESCHEDULABLERUNNABLE_H

#include <thread_highways/execution_tree/Schedule.h>
#include <thread_highways/tools/safe_invoke.h>

#include <cassert>

namespace hi
{

/*
 * A task for execution on highway.
 * This task can reschedule itself.
 * Может использоваться для высокоприоритетных задач:
 * проверка необходимости запуска происходит на каждом такте хайвея
 */
class ReschedulableRunnable
{
public:
	/**
	 * Creating a template task without a protector
	 *
	 * @param r - code to execute (must implements operator())
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note filename and line will used for error and freeze logging
	 */
	template <typename R>
    static ReschedulableRunnable create(R && r, std::chrono::steady_clock::time_point next_execution_time, const char* filename, unsigned int line)
	{
		struct RunnableHolderImpl : public RunnableHolder
		{
			RunnableHolderImpl(R && r)
				: r_{std::move(r)}
			{
			}

			/**
			 * Task execute interface
			 *
			 * @param schedule - Schedule Management Structure, change for schedule
			 * @param global_run_id - identifier with which this highway works now
			 * @param your_run_id - identifier with which this highway was running when this task started
			 * @note if (global_run_id != your_run_id) then you must stop execution
			 * @note using of global_run_id and your_run_id is optional
			 */
			void operator()(
                Schedule& schedule, [[maybe_unused]] const std::atomic<bool>& keep_execution) override
			{
                if constexpr (
                    std::is_invocable_v<R, Schedule &, const std::atomic<bool>&>)
				{
                    r_(schedule, keep_execution);
				}
				else if constexpr (std::is_invocable_v<R, Schedule &>)
				{
					r_(schedule);
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && r = *r_;
                    if constexpr (std::is_invocable_v<decltype(r), Schedule &, const std::atomic<bool>&>)
					{
                        r(schedule, keep_execution);
					}
					else
					{
						r(schedule);
					}
				}
				else
				{
					// The callback signature must be one of the above
					assert(false);
				}
			}
			R r_;
		};
        return ReschedulableRunnable{new RunnableHolderImpl{std::move(r)}, next_execution_time, filename, line};
	}

	/**
	 * Creating a template task with a protector
	 *
	 * @param r - code to execute (must implements operator())
	 * @param protector - object that implements the lock() operator
	 * If the protector.lock() returned false, then the task will not be executed
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note filename and line will used for error and freeze logging
	 */
	template <typename R, typename P>
    static ReschedulableRunnable create(R && runnable, P protector, std::chrono::steady_clock::time_point next_execution_time, const char* filename, unsigned int line)
	{
		struct RunnableProtectedHolderImpl : public RunnableHolder
		{
			RunnableProtectedHolderImpl(R && runnable, P && protector)
				: runnable_{std::move(runnable)}
				, protector_{std::move(protector)}
			{
			}

			/**
			 * Task execute interface
			 *
			 * @param schedule - Schedule Management Structure, change for schedule
			 * @param global_run_id - identifier with which this highway works now
			 * @param your_run_id - identifier with which this highway was running when this task started
			 * @note if (global_run_id != your_run_id) then you must stop execution
			 * @note using of global_run_id and your_run_id is optional
			 */
			void operator()(
                Schedule & schedule, [[maybe_unused]] const std::atomic<bool>& keep_execution) override
			{
                if constexpr (
                    std::is_invocable_v<R, Schedule &, const std::atomic<bool>&>)
				{
                    safe_invoke_void(runnable_, protector_, schedule, keep_execution);
				}
				else if constexpr (std::is_invocable_v<R, Schedule &>)
				{
					safe_invoke_void(runnable_, protector_, schedule);
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && r = *runnable_;
                    if constexpr (std::is_invocable_v<decltype(r), Schedule&, const std::atomic<bool>& >)
					{
                        safe_invoke_void(r, protector_, schedule, keep_execution);
					}
					else
					{
						safe_invoke_void(r, protector_, schedule);
					}
				}
				else
				{
					// The callback signature must be one of the above
					assert(false);
				}
			}
			R runnable_;
			P protector_;
		};
		return ReschedulableRunnable{
			new RunnableProtectedHolderImpl{std::move(runnable), std::move(protector)},
            next_execution_time,
            filename,
			line};
	}

	~ReschedulableRunnable()
	{
		delete runnable_;
	}
	ReschedulableRunnable(const ReschedulableRunnable & rhs) = delete;
	ReschedulableRunnable & operator=(const ReschedulableRunnable & rhs) = delete;
	ReschedulableRunnable(ReschedulableRunnable && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, runnable_{rhs.runnable_}
	{
		rhs.runnable_ = nullptr;
	}
	ReschedulableRunnable & operator=(ReschedulableRunnable && rhs)
	{
		if (this == &rhs)
			return *this;
		filename_ = std::move(rhs.filename_);
		line_ = rhs.line_;
		delete runnable_;
		runnable_ = rhs.runnable_;
		rhs.runnable_ = nullptr;
		return *this;
	}

    void run(const std::atomic<bool>& keep_run)
	{
        (*runnable_)(schedule_, keep_run);
	}

    const char* get_code_filename() const
	{
		return filename_;
	}

	unsigned int get_code_line() const
	{
		return line_;
	}

	Schedule & schedule()
	{
		return schedule_;
	}

private:
	struct RunnableHolder
	{
		virtual ~RunnableHolder() = default;
		virtual void operator()(
			Schedule & schedule,
            const std::atomic<bool>& keep_execution) = 0;
	};

    ReschedulableRunnable(RunnableHolder * runnable, std::chrono::steady_clock::time_point next_execution_time, const char* filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, runnable_{runnable}
	{
        schedule_.next_execution_time_ = next_execution_time;
	}

    const char* filename_;
	unsigned int line_;
	RunnableHolder * runnable_{nullptr};
	Schedule schedule_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_RESCHEDULABLERUNNABLE_H
