/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef ReschedulableRunnable_H
#define ReschedulableRunnable_H

#include <thread_highways/tools/safe_invoke.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <string>

namespace hi
{

/*
 * A task for execution on highway.
 * This task can reschedule itself.
 *
 * For single threaded use only
 */
class ReschedulableRunnable
{
public:
	// Schedule Management Structure
	struct Schedule
	{
		// true == should be rescheduled. Automatically set to false before each run
		bool rechedule_{false};

		// Point in time after which the task should be launched for execution
		std::chrono::steady_clock::time_point next_execution_time_{}; // == run if less then now()

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
	};

	/**
	 * Creating a template task without a protector
	 *
	 * @param r - code to execute (must implements operator())
	 * @param filename - file where the code is located
	 * @param line - line in the file that contains the code
	 * @note filename and line will used for error and freeze logging
	 */
	template <typename R>
	static ReschedulableRunnable create(R && r, std::string filename, unsigned int line)
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
				Schedule & schedule,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (
					std::is_invocable_v<R, Schedule &, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					r_(schedule, global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Schedule &>)
				{
					r_(schedule);
				}
				else
				{
					// The callback signature must be one of the above
					assert(false);
				}
			}
			R r_;
		};
		return ReschedulableRunnable{new RunnableHolderImpl{std::move(r)}, std::move(filename), line};
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
	static ReschedulableRunnable create(R && runnable, P protector, std::string filename, unsigned int line)
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
				Schedule & schedule,
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (
					std::is_invocable_v<R, Schedule &, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					safe_invoke_void(runnable_, protector_, schedule, global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R, Schedule &>)
				{
					safe_invoke_void(runnable_, protector_, schedule);
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
			std::move(filename),
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

	void run(const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id)
	{
		(*runnable_)(schedule_, global_run_id, your_run_id);
	}

	std::string get_code_filename() const
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
			const std::atomic<std::uint32_t> & global_run_id,
			const std::uint32_t your_run_id) = 0;
	};

	ReschedulableRunnable(RunnableHolder * runnable, std::string filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, runnable_{runnable}
	{
	}

	std::string filename_;
	unsigned int line_;
	RunnableHolder * runnable_{nullptr};
	Schedule schedule_;
};

} // namespace hi

#endif // ReschedulableRunnable_H
