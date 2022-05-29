/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef RUNNABLE_H
#define RUNNABLE_H

#include <thread_highways/tools/safe_invoke.h>

#include <atomic>
#include <cassert>
#include <string>

namespace hi
{

// A task for execution on highway.
class Runnable
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
	static Runnable create(R && r, std::string filename, unsigned int line)
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
			 * @param global_run_id - identifier with which this highway works now
			 * @param your_run_id - identifier with which this highway was running when this task started
			 * @note if (global_run_id != your_run_id) then you must stop execution
			 * @note using of global_run_id and your_run_id is optional
			 */
			void operator()(
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<R, LaunchParameters>)
				{
					r_(LaunchParameters{global_run_id, your_run_id});
				}
				else if constexpr (std::is_invocable_v<R, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					r_(global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R>)
				{
					r_();
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && r = *r_;
					if constexpr (std::is_invocable_v<decltype(r), LaunchParameters>)
					{
						r(LaunchParameters{global_run_id, your_run_id});
					}
					else if constexpr (std::is_invocable_v<
										   decltype(r),
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						r(global_run_id, your_run_id);
					}
					else
					{
						r();
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
		return Runnable{new RunnableHolderImpl{std::move(r)}, std::move(filename), line};
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
	static Runnable create(R && runnable, P protector, std::string filename, unsigned int line)
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
			 * @param global_run_id - identifier with which this highway works now
			 * @param your_run_id - identifier with which this highway was running when this task started
			 * @note if (global_run_id != your_run_id) then you must stop execution
			 * @note using of global_run_id and your_run_id is optional
			 */
			void operator()(
				[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
				[[maybe_unused]] const std::uint32_t your_run_id) override
			{
				if constexpr (std::is_invocable_v<R, LaunchParameters>)
				{
					safe_invoke_void(runnable_, protector_, LaunchParameters{global_run_id, your_run_id});
				}
				else if constexpr (std::is_invocable_v<R, const std::atomic<std::uint32_t> &, const std::uint32_t>)
				{
					safe_invoke_void(runnable_, protector_, global_run_id, your_run_id);
				}
				else if constexpr (std::is_invocable_v<R>)
				{
					safe_invoke_void(runnable_, protector_);
				}
				else if constexpr (can_be_dereferenced<R &>::value)
				{
					auto && r = *runnable_;
					if constexpr (std::is_invocable_v<decltype(r), LaunchParameters>)
					{
						safe_invoke_void(r, protector_, LaunchParameters{global_run_id, your_run_id});
					}
					else if constexpr (std::is_invocable_v<
										   decltype(r),
										   const std::atomic<std::uint32_t> &,
										   const std::uint32_t>)
					{
						safe_invoke_void(r, protector_, global_run_id, your_run_id);
					}
					else
					{
						safe_invoke_void(r, protector_);
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
		return Runnable{
			new RunnableProtectedHolderImpl{std::move(runnable), std::move(protector)},
			std::move(filename),
			line};
	}

	~Runnable()
	{
		delete runnable_;
	}
	Runnable(const Runnable & rhs) = delete;
	Runnable & operator=(const Runnable & rhs) = delete;
	Runnable(Runnable && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, runnable_{rhs.runnable_}
	{
		rhs.runnable_ = nullptr;
	}
	Runnable & operator=(Runnable && rhs)
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

	/**
	 * @brief The LaunchParameters struct
	 * The structure groups all incoming parameters for convenience.
	 * This is convenient because with an increase in the number of
	 * incoming parameters, you will not have to make changes to previously developed callbacks.
	 */
	struct LaunchParameters
	{
		// identifier with which this highway works now
		const std::reference_wrapper<const std::atomic<std::uint32_t>> global_run_id_;
		// your_run_id - identifier with which this highway was running when this task started
		const std::uint32_t your_run_id_;
	};

	void run(const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id)
	{
		(*runnable_)(global_run_id, your_run_id);
	}

	std::string get_code_filename() const
	{
		return filename_;
	}

	unsigned int get_code_line() const
	{
		return line_;
	}

private:
	struct RunnableHolder
	{
		virtual ~RunnableHolder() = default;
		virtual void operator()(const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id) = 0;
	};

	Runnable(RunnableHolder * runnable, std::string filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, runnable_{runnable}
	{
	}

	std::string filename_;
	unsigned int line_;
	RunnableHolder * runnable_{nullptr};
};

} // namespace hi

#endif // RUNNABLE_H
