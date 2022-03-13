#ifndef RUNNABLE_H
#define RUNNABLE_H

#include <thread_highways/tools/safe_invoke.h>

#include <atomic>
#include <string>

namespace hi
{

/*
	Any Runnable
	Has the right to execute while the your_run_id == highway_bundle.global_run_id_
*/
class Runnable
{
public:
	template <typename R>
	static Runnable create(R && r, std::string filename, unsigned int line)
	{
		struct RunnableHolderImpl : public RunnableHolder
		{
			RunnableHolderImpl(R && r)
				: r_{std::move(r)}
			{
			}
			void operator()(const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id) override
			{
				r_(global_run_id, your_run_id);
			}
			R r_;
		};
		return Runnable{new RunnableHolderImpl{std::move(r)}, std::move(filename), line};
	}

	template <typename R, typename P>
	static Runnable create(R && runnable, P && protector, std::string filename, unsigned int line)
	{
		struct RunnableProtectedHolderImpl : public RunnableHolder
		{
			RunnableProtectedHolderImpl(R && runnable, P && protector)
				: runnable_{std::move(runnable)}
				, protector_{std::move(protector)}
			{
			}

			void operator()(const std::atomic<std::uint32_t> & global_run_id, const std::uint32_t your_run_id) override
			{
				safe_invoke_void(runnable_, protector_, global_run_id, your_run_id);
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
		runnable_ = rhs.runnable_;
		rhs.runnable_ = nullptr;
		return *this;
	}

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
	RunnableHolder * runnable_;
};

} // namespace hi

#endif // RUNNABLE_H
