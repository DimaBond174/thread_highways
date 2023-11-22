#ifndef THREADS_HIGHWAYS_HIGHWAYS_SCHEDULER_H
#define THREADS_HIGHWAYS_HIGHWAYS_SCHEDULER_H

#include <thread_highways/execution_tree/ReschedulableRunnable.h>
#include <thread_highways/tools/stack.h>

#include <atomic>

namespace hi
{

// Создаётся на стэке MainThread
// определяет сколько может проспать MainThread

// todo - не нужен: экономим на вызове функции - пусть сразу schedule_stack_ будет на стеке
class Scheduler
{
public:
	void add_reschedulable_runnable(ReschedulableRunnable && runnable)
	{
		const auto schedule = runnable.schedule();
		if (schedule.next_execution_time_ < next_execution_time_)
		{
			next_execution_time_ = schedule.next_execution_time_;
		}
		schedule_stack_.push(new hi::Holder<ReschedulableRunnable>(std::move(runnable)));
	}

	// возвращает время до которого можно спать
	// если задач нет, то +1 день к текущему времени
	std::chrono::steady_clock::time_point execute(const std::atomic<bool> & keep_execution_)
	{
		if (schedule_stack_.empty())
		{
			return std::chrono::steady_clock::now() + std::chrono::days{1};
		}
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
	}

	// возвращает время до которого можно спать
	// если задач нет, то +1 день к текущему времени
	std::chrono::steady_clock::time_point execute_with_time_control(const RunBundle & bundle)
	{
		if (schedule_stack_.empty())
		{
			return std::chrono::steady_clock::now() + std::chrono::days{1};
		}
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
	}

private:
	// Запланированные задачи
	SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack_;
	// Point in time after which the task should be launched for execution
	std::chrono::steady_clock::time_point next_execution_time_{}; // == run if less then now()
};

} // namespace hi

#endif // THREADS_HIGHWAYS_HIGHWAYS_SCHEDULER_H
