/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_HIGHWAYS_HIGHWAY_ABA_SAFE_H
#define THREADS_HIGHWAYS_HIGHWAYS_HIGHWAY_ABA_SAFE_H

#include <thread_highways/execution_tree/runnable.h>
#include <thread_highways/execution_tree/reschedulable_runnable.h>
#include <thread_highways/mailboxes/mail_box_aba_safe.h>
#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/raii_thread.h>
#include <thread_highways/tools/stack.h>

#include <chrono>
#include <functional>
#include <future>

namespace hi
{

class HighWayAbaSafe
{
public:
	HighWayAbaSafe(
		std::weak_ptr<HighWayAbaSafe> self_weak,
		std::uint32_t mail_box_capacity = 65000u,
		ExceptionHandler exception_handler =
			[](const hi::Exception & ex)
		{
			throw ex;
		},
		std::string highway_name = "HighWayAbaSafe",
		std::chrono::milliseconds max_task_execution_time = {},
		std::shared_ptr<MailBoxAbaSafe<Runnable>> multi_thread_mail_box = nullptr)
		: self_weak_{std::move(self_weak)}
		, exception_handler_{std::move(exception_handler)}
		, highway_name_{std::move(highway_name)}
		, max_task_execution_time_{max_task_execution_time}
		, multi_thread_mail_box_{std::move(multi_thread_mail_box)}
		, mail_box_{mail_box_capacity}
	{
		next_schedule_time_ = std::chrono::steady_clock::now() + std::chrono::hours{24};
		if (multi_thread_mail_box_)
		{
			if (max_task_execution_time_ == std::chrono::milliseconds{})
			{
				main_thread_ = RAIIthread(std::thread(
					[this, self_protector = self_weak_.lock()]
					{
						main_loop_without_time_control_multi(self_protector);
					}));
			}
			else
			{
				main_thread_ = RAIIthread(std::thread(
					[this, self_protector = self_weak_.lock()]
					{
						main_loop_with_time_control_multi(self_protector);
					}));
			}
		}
		else
		{
			if (max_task_execution_time_ == std::chrono::milliseconds{})
			{
				main_thread_ = RAIIthread(std::thread(
					[this, self_protector = self_weak_.lock()]
					{
						main_loop_without_time_control(self_protector);
					}));
			}
			else
			{
				main_thread_ = RAIIthread(std::thread(
					[this, self_protector = self_weak_.lock()]
					{
						main_loop_with_time_control(self_protector);
					}));
			}
		}
	}

	// Будет пытаться добавить задачу, если ресурсов не осталось то заблокируется в ожидании
	void execute(Runnable && runnable)
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
	void execute(R && r, const char * filename = __FILE__, const unsigned int line = __LINE__)
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
	void execute(R && r, P protector, const char * filename, const unsigned int line)
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
	bool try_execute(R && r, const char * filename = __FILE__, const unsigned int line = __LINE__)
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
	bool try_execute(R && r, P protector, const char * filename, const unsigned int line)
	{
		return try_execute_impl(Runnable::create<R>(std::move(r), std::move(protector), filename, line));
	}

	void schedule(ReschedulableRunnable && runnable)
	{
		schedule_impl(std::move(runnable));
	}

	template <typename R>
	void schedule(
		R && r,
		std::chrono::steady_clock::time_point next_execution_time = {},
		const char * filename = __FILE__,
		const unsigned int line = __LINE__)
	{
		schedule_impl(ReschedulableRunnable::create(std::move(r), next_execution_time, filename, line));
	}
	template <typename R, typename P>
	void schedule(
		R && r,
		P protector,
		std::chrono::steady_clock::time_point next_execution_time,
		const char * filename,
		const unsigned int line)
	{
		schedule_impl(
			ReschedulableRunnable::create(std::move(r), std::move(protector), next_execution_time, filename, line));
	}

	void destroy()
	{
		keep_execution_.store(false, std::memory_order_release);
		mail_box_.destroy();
		main_thread_.join();
	}

	// отработать всё что скопилось в mailbox
	// рекомендуется использовать только для отладки
	void flush_tasks()
	{
		std::promise<bool> last_task;
		auto last_task_executed = last_task.get_future();

		mail_box_.send_may_blocked(hi::Runnable::create(
			[&]
			{
				last_task.set_value(true);
			},
			self_weak_,
			__FILE__,
			__LINE__));
		if (last_task_executed.get())
		{
			return;
		}
	} // flush_tasks

private:
	void execute_impl(Runnable && runnable)
	{
		mail_box_.send_may_blocked(std::move(runnable));
	}

	bool try_execute_impl(Runnable && runnable)
	{
		return mail_box_.send_may_fail(std::move(runnable));
	}

	void schedule_impl(ReschedulableRunnable && runnable)
	{
		execute(
			[&, runnable = std::move(runnable)]() mutable
			{
				if (next_schedule_time_ > runnable.schedule().next_execution_time_)
				{
					next_schedule_time_ = runnable.schedule().next_execution_time_;
				}
				schedule_stack_.push(new hi::Holder<ReschedulableRunnable>(std::move(runnable)));
			});
	}

	void main_loop_without_time_control(const std::shared_ptr<HighWayAbaSafe> self_protector)
	{
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
		std::stack<Runnable> work_queue;
		auto time = std::chrono::steady_clock::now();
		const auto execute_reschedulable_runnable = [&](Holder<ReschedulableRunnable> * holder)
		{
			try
			{
				holder->t_.schedule().rechedule_ = false;
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}

			if (holder->t_.schedule().rechedule_)
			{
				if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
				{
					next_schedule_time_ = holder->t_.schedule().next_execution_time_;
				}
				schedule_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		};

		const auto check_schedules = [&]
		{
			time = std::chrono::steady_clock::now();
			if (time >= next_schedule_time_)
			{
				next_schedule_time_ = time + std::chrono::hours{24};
				while (auto holder = schedule_stack_.pop())
				{
					if (time >= holder->t_.schedule().next_execution_time_)
					{
						execute_reschedulable_runnable(holder);
					}
					else
					{
						if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
						{
							next_schedule_time_ = holder->t_.schedule().next_execution_time_;
						}
						schedule_stack.push(holder);
					}
					if (!keep_execution_.load(std::memory_order_acquire))
					{
						return;
					}
				} // while schedule_stack_
				schedule_stack_.swap(schedule_stack);
			} // if (before_time >=  next_schedule_time_)
		};

		const auto execute_runnable = [&](Runnable & runnable)
		{
			try
			{
				runnable.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
		};

		// main loop
		while (keep_execution_.load(std::memory_order_acquire))
		{
			time = std::chrono::steady_clock::now();

			mail_box_.move_stack(
				work_queue,
				std::chrono::duration_cast<std::chrono::nanoseconds>(next_schedule_time_ - time));
			check_schedules();
			while (keep_execution_.load(std::memory_order_acquire) && !work_queue.empty())
			{
				check_schedules();
				execute_runnable(work_queue.top());
				work_queue.pop();
			}
		} // while main loop

		self_protector->keep_execution_ = false;
	} // main_loop_without_time_control

	void main_loop_with_time_control(const std::shared_ptr<HighWayAbaSafe> self_protector)
	{
		auto before_time = std::chrono::steady_clock::now();
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
		std::stack<Runnable> work_queue;
		const auto execute_reschedulable_runnable = [&](Holder<ReschedulableRunnable> * holder)
		{
			try
			{
				holder->t_.schedule().rechedule_ = false;
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			before_time = after_time;
			if (time_diff > max_task_execution_time_)
			{
				exception_handler_(hi::Exception{
					highway_name_ + ":ReschedulableRunnable:stuck for ms = " + std::to_string(time_diff.count()),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line()});
			}

			if (holder->t_.schedule().rechedule_)
			{
				if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
				{
					next_schedule_time_ = holder->t_.schedule().next_execution_time_;
				}
				schedule_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		};

		const auto check_schedules = [&]
		{
			if (before_time >= next_schedule_time_)
			{
				next_schedule_time_ = before_time + std::chrono::hours{24};
				while (auto holder = schedule_stack_.pop())
				{
					if (before_time >= holder->t_.schedule().next_execution_time_)
					{
						execute_reschedulable_runnable(holder);
					}
					else
					{
						if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
						{
							next_schedule_time_ = holder->t_.schedule().next_execution_time_;
						}
						schedule_stack.push(holder);
					}
					if (!keep_execution_.load(std::memory_order_acquire))
					{
						return;
					}
				} // while schedule_stack_
				schedule_stack_.swap(schedule_stack);
			} // if (before_time >=  next_schedule_time_)
		};

		const auto execute_runnable = [&](Runnable & runnable)
		{
			before_time = std::chrono::steady_clock::now();
			try
			{
				runnable.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			before_time = after_time;
			if (time_diff > max_task_execution_time_)
			{
				exception_handler_(hi::Exception{
					highway_name_ + ":Runnable:stuck for ms = " + std::to_string(time_diff.count()),
					runnable.get_code_filename(),
					runnable.get_code_line()});
			}
		};

		// main loop
		while (keep_execution_.load(std::memory_order_acquire))
		{
			mail_box_.move_stack(
				work_queue,
				std::chrono::duration_cast<std::chrono::nanoseconds>(next_schedule_time_ - before_time));
			check_schedules();
			while (keep_execution_.load(std::memory_order_acquire) && !work_queue.empty())
			{
				check_schedules();
				execute_runnable(work_queue.top());
				work_queue.pop();
			}
		} // while main loop

		self_protector->keep_execution_ = false;
	} // main_loop_with_time_control

	void main_loop_without_time_control_multi(const std::shared_ptr<HighWayAbaSafe> self_protector)
	{
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
		std::stack<Runnable> work_queue;
		auto time = std::chrono::steady_clock::now();
		const auto execute_reschedulable_runnable = [&](Holder<ReschedulableRunnable> * holder)
		{
			try
			{
				holder->t_.schedule().rechedule_ = false;
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}

			if (holder->t_.schedule().rechedule_)
			{
				if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
				{
					next_schedule_time_ = holder->t_.schedule().next_execution_time_;
				}
				schedule_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		};

		const auto check_schedules = [&]
		{
			time = std::chrono::steady_clock::now();
			if (time >= next_schedule_time_)
			{
				next_schedule_time_ = time + std::chrono::hours{24};
				while (auto holder = schedule_stack_.pop())
				{
					if (time >= holder->t_.schedule().next_execution_time_)
					{
						execute_reschedulable_runnable(holder);
					}
					else
					{
						if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
						{
							next_schedule_time_ = holder->t_.schedule().next_execution_time_;
						}
						schedule_stack.push(holder);
					}
					if (!keep_execution_.load(std::memory_order_acquire))
					{
						return;
					}
				} // while schedule_stack_
				schedule_stack_.swap(schedule_stack);
			} // if (before_time >=  next_schedule_time_)
		};

		const auto execute_runnable = [&](Runnable & runnable)
		{
			try
			{
				runnable.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
		};

		// main loop
		while (keep_execution_.load(std::memory_order_acquire))
		{
			mail_box_.move_stack_no_wait(work_queue);
			check_schedules();

			if (work_queue.empty())
			{
				if (exec_one_from_multi_thread_mail_box())
				{
					continue;
				}
				else
				{
					time = std::chrono::steady_clock::now();
					auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(next_schedule_time_ - time);
					if (wait_time > permissible_delay_time_)
						wait_time = permissible_delay_time_;
					mail_box_.move_stack(work_queue, wait_time);
				}
			}

			while (keep_execution_.load(std::memory_order_acquire) && !work_queue.empty())
			{
				check_schedules();
				execute_runnable(work_queue.top());
				work_queue.pop();
			}
		} // while main loop

		self_protector->keep_execution_ = false;
	} // main_loop_without_time_control_multi

	void main_loop_with_time_control_multi(const std::shared_ptr<HighWayAbaSafe> self_protector)
	{
		auto before_time = std::chrono::steady_clock::now();
		SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack;
		std::stack<Runnable> work_queue;
		const auto execute_reschedulable_runnable = [&](Holder<ReschedulableRunnable> * holder)
		{
			try
			{
				holder->t_.schedule().rechedule_ = false;
				holder->t_.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			before_time = after_time;
			if (time_diff > max_task_execution_time_)
			{
				exception_handler_(hi::Exception{
					highway_name_ + ":ReschedulableRunnable:stuck for ms = " + std::to_string(time_diff.count()),
					holder->t_.get_code_filename(),
					holder->t_.get_code_line()});
			}

			if (holder->t_.schedule().rechedule_)
			{
				if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
				{
					next_schedule_time_ = holder->t_.schedule().next_execution_time_;
				}
				schedule_stack.push(holder);
			}
			else
			{
				delete holder;
			}
		};

		const auto check_schedules = [&]
		{
			if (before_time >= next_schedule_time_)
			{
				next_schedule_time_ = before_time + std::chrono::hours{24};
				while (auto holder = schedule_stack_.pop())
				{
					if (before_time >= holder->t_.schedule().next_execution_time_)
					{
						execute_reschedulable_runnable(holder);
					}
					else
					{
						if (next_schedule_time_ > holder->t_.schedule().next_execution_time_)
						{
							next_schedule_time_ = holder->t_.schedule().next_execution_time_;
						}
						schedule_stack.push(holder);
					}
					if (!keep_execution_.load(std::memory_order_acquire))
					{
						return;
					}
				} // while schedule_stack_
				schedule_stack_.swap(schedule_stack);
			} // if (before_time >=  next_schedule_time_)
		};

		const auto execute_runnable = [&](Runnable & runnable)
		{
			before_time = std::chrono::steady_clock::now();
			try
			{
				runnable.run(keep_execution_);
			}
			catch (const hi::Exception & e)
			{
				exception_handler_(e);
			}
			catch (...)
			{
				exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
			}
			const auto after_time = std::chrono::steady_clock::now();
			const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);
			before_time = after_time;
			if (time_diff > max_task_execution_time_)
			{
				exception_handler_(hi::Exception{
					highway_name_ + ":Runnable:stuck for ms = " + std::to_string(time_diff.count()),
					runnable.get_code_filename(),
					runnable.get_code_line()});
			}
		};

		// main loop
		while (keep_execution_.load(std::memory_order_acquire))
		{
			mail_box_.move_stack_no_wait(work_queue);
			check_schedules();

			if (work_queue.empty())
			{
				if (exec_one_from_multi_thread_mail_box_with_time_control())
				{
					continue;
				}
				else
				{
					auto wait_time =
						std::chrono::duration_cast<std::chrono::nanoseconds>(next_schedule_time_ - before_time);
					if (wait_time > permissible_delay_time_)
						wait_time = permissible_delay_time_;
					mail_box_.move_stack(work_queue, wait_time);
				}
			}

			while (keep_execution_.load(std::memory_order_acquire) && !work_queue.empty())
			{
				check_schedules();
				execute_runnable(work_queue.top());
				work_queue.pop();
			}
		} // while main loop

		self_protector->keep_execution_ = false;
	} // main_loop_with_time_control_multi

	bool exec_one_from_multi_thread_mail_box()
	{
		auto holder = multi_thread_mail_box_->pop_message_no_wait();
		if (!holder)
			return false;

		try
		{
			holder->t_.run(keep_execution_);
		}
		catch (const hi::Exception & e)
		{
			exception_handler_(e);
		}
		catch (...)
		{
			exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
		}

		multi_thread_mail_box_->free_work_queue_holder(*holder);
		return true;
	}

	bool exec_one_from_multi_thread_mail_box_with_time_control()
	{
		auto holder = multi_thread_mail_box_->pop_message_no_wait();
		if (!holder)
			return false;
		const auto before_time = std::chrono::steady_clock::now();
		try
		{
			holder->t_.run(keep_execution_);
		}
		catch (const hi::Exception & e)
		{
			exception_handler_(e);
		}
		catch (...)
		{
			exception_handler_(hi::Exception{highway_name_ + ": ", __FILE__, __LINE__, std::current_exception()});
		}
		const auto after_time = std::chrono::steady_clock::now();
		const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(after_time - before_time);

		if (time_diff > max_task_execution_time_)
		{
			exception_handler_(hi::Exception{
				highway_name_ + ":Runnable:stuck for ms = " + std::to_string(time_diff.count()),
				holder->t_.get_code_filename(),
				holder->t_.get_code_line()});
		}

		multi_thread_mail_box_->free_work_queue_holder(*holder);
		return true;
	}

private:
	const std::weak_ptr<HighWayAbaSafe> self_weak_;
	const ExceptionHandler exception_handler_;
	const std::string highway_name_;

	// для шедулера параметр не нужен т.к. он сам считает сколько можно проспать до следующего старта
	// а задачи приходят через семафор без сна

	// если задан, то будет контроль времени исполнения - всё что дольше попадёт в exception_handler_
	const std::chrono::milliseconds max_task_execution_time_;
	const std::shared_ptr<MailBoxAbaSafe<Runnable>> multi_thread_mail_box_;
	static constexpr std::chrono::nanoseconds permissible_delay_time_{100000000};

	// одноразовый рубильник
	std::atomic<bool> keep_execution_{true};

	RAIIthread main_thread_;
	MailBoxAbaSafe<Runnable> mail_box_;

private: // main_thread_ thread local:
	// Запланированные задачи
	SingleThreadStack<Holder<ReschedulableRunnable>> schedule_stack_;
	// Point in time after which the task should be launched for execution
	std::chrono::steady_clock::time_point next_schedule_time_{}; // == run if less then now()
};

} // namespace hi

#endif // THREADS_HIGHWAYS_HIGHWAYS_HIGHWAY_ABA_SAFE_H
