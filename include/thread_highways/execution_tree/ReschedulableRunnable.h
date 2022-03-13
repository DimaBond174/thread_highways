#ifndef ReschedulableRunnable_H
#define ReschedulableRunnable_H

#include <thread_highways/tools/safe_invoke.h>

#include <atomic>
#include <chrono>
#include <string>

namespace hi
{

/*
	Runnable with self scheduling
	Has the right to execute while the your_run_id == highway_bundle.global_run_id_
		schedule  allows you to manage rescheduling yourself
*/
class ReschedulableRunnable
{
public:
	struct Schedule
	{
		bool rechedule_{false}; // == false before each run()
		std::chrono::steady_clock::time_point next_execution_time_{}; // == run if less then now()

		void schedule_launch_in(const std::chrono::milliseconds ms)
		{
			rechedule_ = true;
			next_execution_time_ = std::chrono::steady_clock::now() + ms;
		}
	};

	template <typename R>
	static ReschedulableRunnable create(R && r, std::string filename, unsigned int line)
	{
		struct RunnableHolderImpl : public RunnableHolder
		{
			RunnableHolderImpl(R && r)
				: r_{std::move(r)}
			{
			}
			void operator()(
				Schedule & schedule,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id) override
			{
				r_(schedule, global_run_id, your_run_id);
			}
			R r_;
		};
		return ReschedulableRunnable{new RunnableHolderImpl{std::move(r)}, std::move(filename), line};
	}

	template <typename R, typename P>
	static ReschedulableRunnable create(R && runnable, P && protector, std::string filename, unsigned int line)
	{
		struct RunnableProtectedHolderImpl : public RunnableHolder
		{
			RunnableProtectedHolderImpl(R && runnable, P && protector)
				: runnable_{std::move(runnable)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(
				Schedule & schedule,
				const std::atomic<std::uint32_t> & global_run_id,
				const std::uint32_t your_run_id) override
			{
				safe_invoke_void(runnable_, protector_, schedule, global_run_id, your_run_id);
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
	RunnableHolder * runnable_;
	Schedule schedule_;
};

} // namespace hi

#endif // ReschedulableRunnable_H
