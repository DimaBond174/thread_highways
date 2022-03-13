#ifndef IHIGHWAY_H
#define IHIGHWAY_H

#include <thread_highways/execution_tree/Runnable.h>
#include <thread_highways/mailboxes/IMailBoxSendHere.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/error_logger.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace hi
{

using IHighWayMailBoxPtr = std::shared_ptr<IMailBoxSendHere<Runnable>>;

struct HighWayBundle
{
	HighWayBundle(
		std::shared_ptr<std::atomic<std::uint32_t>> global_run_id,
		std::string highway_name,
		ErrorLoggerPtr error_logger,
		std::chrono::milliseconds max_task_execution_time)
		: global_run_id_{global_run_id ? std::move(global_run_id) : std::make_shared<std::atomic<std::uint32_t>>(0)}
		, highway_name_{highway_name}
		, error_logger_{error_logger}
		, max_task_execution_time_{max_task_execution_time}
		, mail_box_{
			  global_run_id_,
			  error_logger ? std::make_shared<ErrorLogger>(
				  [error_logger = std::move(error_logger), highway_name = std::move(highway_name)](std::string err)
				  {
					  (*error_logger)(std::string(highway_name).append(": ").append(std::move(err)));
				  })
						   : error_logger}
	{
		assert(global_run_id_);
	}

	[[nodiscard]] bool is_stuck() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
				   std::chrono::steady_clock::now() - task_start_time_.load(std::memory_order_acquire))
			> max_task_execution_time_;
	}

	void log(const std::string & what, const std::string & code_filename, unsigned int code_line)
	{
		if (!error_logger_)
		{
			return;
		}
		using namespace std::chrono;
		std::stringstream ss;
		ss << highway_name_ << ":thread(" << std::this_thread::get_id() << "):milliseconds("
		   << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() << "):code_filename("
		   << code_filename << "):code_line(" << code_line << "):what:" << what;

		(*error_logger_)(ss.str());
	}

	const std::shared_ptr<std::atomic<std::uint32_t>> global_run_id_;
	const std::string highway_name_;
	const ErrorLoggerPtr error_logger_;
	const std::chrono::milliseconds max_task_execution_time_;

	enum class WhatRunningNow
	{
		Sleep,
		MailBoxMessage,
		MainLoopLogic,
		OtherLogic,
		Stopped
	};
	std::atomic<WhatRunningNow> what_is_running_now_{WhatRunningNow::Sleep};
	std::atomic<std::chrono::time_point<std::chrono::steady_clock>> task_start_time_{};
	MailBox<Runnable> mail_box_;
};

/*
	Highway's main loop body
	Has the right to execute while the your_run_id == highway_bundle.global_run_id_
*/
class HighWayMainLoopRunnable
{
public:
	template <typename R>
	static HighWayMainLoopRunnable create(R && r, std::string filename, unsigned int line)
	{
		struct RunnableHolderImpl : public RunnableHolder
		{
			RunnableHolderImpl(R && r)
				: r_{std::move(r)}
			{
			}
			void operator()(HighWayBundle & highway_bundle, const std::uint32_t your_run_id) override
			{
				r_(highway_bundle, your_run_id);
			}
			R r_;
		};
		return HighWayMainLoopRunnable{new RunnableHolderImpl{std::move(r)}, std::move(filename), line};
	}

	~HighWayMainLoopRunnable()
	{
		delete runnable_;
	}
	HighWayMainLoopRunnable(const HighWayMainLoopRunnable & rhs) = delete;
	HighWayMainLoopRunnable & operator=(const HighWayMainLoopRunnable & rhs) = delete;
	HighWayMainLoopRunnable(HighWayMainLoopRunnable && rhs)
		: filename_{std::move(rhs.filename_)}
		, line_{rhs.line_}
		, runnable_{rhs.runnable_}
	{
		rhs.runnable_ = nullptr;
	}
	HighWayMainLoopRunnable & operator=(HighWayMainLoopRunnable && rhs)
	{
		if (this == &rhs)
			return *this;
		filename_ = std::move(rhs.filename_);
		line_ = rhs.line_;
		runnable_ = rhs.runnable_;
		rhs.runnable_ = nullptr;
		return *this;
	}

	void run(HighWayBundle & highway_bundle, const std::uint32_t your_run_id)
	{
		(*runnable_)(highway_bundle, your_run_id);
	}

	std::string get_code_filename()
	{
		return filename_;
	}

	unsigned int get_code_line()
	{
		return line_;
	}

private:
	struct RunnableHolder
	{
		virtual ~RunnableHolder() = default;
		virtual void operator()(HighWayBundle & highway_bundle, const std::uint32_t your_run_id) = 0;
	};

	HighWayMainLoopRunnable(RunnableHolder * runnable, std::string filename, unsigned int line)
		: filename_{std::move(filename)}
		, line_{line}
		, runnable_{runnable}
	{
	}

	std::string filename_;
	unsigned int line_;
	RunnableHolder * runnable_;
};

class IHighWay
{
public:
	/*
	global_keep_run может быть персональным для каждого хайвея
	  или один на всех или на группу хайвеев.
	*/
	IHighWay(
		std::shared_ptr<IHighWay> self_protector,
		std::shared_ptr<std::atomic<std::uint32_t>> global_keep_run,
		std::string highway_name,
		ErrorLoggerPtr error_logger,
		std::chrono::milliseconds max_task_execution_time)
		: self_weak_{self_protector}
		, bundle_{std::move(global_keep_run), std::move(highway_name), std::move(error_logger), max_task_execution_time}
		, self_protector_{std::move(self_protector)}
	{
	}
	virtual ~IHighWay() = default;

	std::weak_ptr<IHighWay> protector()
	{
		return self_weak_;
	}

	IHighWayMailBoxPtr mailbox()
	{
		struct MailBoxSendHere : public IMailBoxSendHere<Runnable>
		{
			MailBoxSendHere(std::weak_ptr<IHighWay> self_weak)
				: weak_{std::move(self_weak)}
			{
			}

			bool send_may_fail(Runnable && r) override
			{
				if (auto lock = weak_.lock())
				{
					return lock->bundle_.mail_box_.send_may_fail(std::move(r));
				}
				return false;
			}

			bool send_may_blocked(Runnable && r) override
			{
				if (auto lock = weak_.lock())
				{
					lock->bundle_.mail_box_.send_may_blocked(std::move(r));
					return true;
				}
				return false;
			}

			std::weak_ptr<IHighWay> weak_;
		};

		return std::make_shared<MailBoxSendHere>(self_weak_);
	}

	std::string get_name()
	{
		return bundle_.highway_name_;
	}

	// Должен обязательно вызываться для удаления
	// Хайвей держит shared_ptr на себя чтобы не был случайно вызван деструктор до
	// того момента когда рабочий поток остановлен.
	virtual void destroy() = 0;

	// Запуск самопроверки хайвея
	// если вернуть false, значит хайвей больше не хочет чтобы его мониторили
	// (например если уже был вызван destroy())
	virtual bool self_check() = 0;

	// Установка максимального количества сообщений которые могут обрабатываться хайвеем в моменте
	// Рекомендуется устанавливать в соответствии с ограничениями на RAM - если количество сообщений
	// может бесконечно расти, то это может забить любой объём RAM (например если потребитель сообщений
	// обрабатывает сообщения медленнее чем производители сообщений их производят)
	void set_capacity(std::uint32_t capacity)
	{
		bundle_.mail_box_.set_capacity(capacity);
	}

	// отработать всё что скопилось в mailbox
	// рекомендуется использовать только для отладки
	void flush_tasks()
	{
		std::promise<bool> last_task;
		auto last_task_executed = last_task.get_future();

		bundle_.mail_box_.send_may_blocked(hi::Runnable::create(
			[&](const std::atomic<std::uint32_t> &, const std::uint32_t)
			{
				last_task.set_value(true);
			},
			std::weak_ptr{self_protector_},
			__FILE__,
			__LINE__));
		if (last_task_executed.get())
		{
			return;
		}
	}

protected:
	const std::weak_ptr<IHighWay> self_weak_;
	HighWayBundle bundle_;
	std::shared_ptr<IHighWay> self_protector_;

	// todo подключение к каналу в который слать ошибки/падения/фризы
	// по фризам нужен антиФриз дополнительный поток который будет следить за сроками в основном(ых) потоках
};

} // namespace hi

#endif // IHIGHWAY_H
