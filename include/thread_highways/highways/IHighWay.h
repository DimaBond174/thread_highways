#ifndef IHIGHWAY_H
#define IHIGHWAY_H

#include <thread_highways/execution_tree/Runnable.h>
#include <thread_highways/mailboxes/IMailBoxSendHere.h>
#include <thread_highways/mailboxes/MailBox.h>
#include <thread_highways/tools/logger.h>
#include <thread_highways/tools/small_tools.h>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>

namespace hi
{

using IHighWayMailBoxPtr = std::shared_ptr<IMailBoxSendHere<Runnable>>;

struct HighWayBundle
{
	HighWayBundle(std::string highway_name, LoggerPtr logger, std::chrono::milliseconds max_task_execution_time)
		: highway_name_{std::move(highway_name)}
		, logger_{std::move(logger)}
		, max_task_execution_time_{max_task_execution_time}
		, task_start_time_{steady_clock_now()}
		, mail_box_{[this](auto &&... args)
					{
						log(args...);
					}}
	{
	}

	[[nodiscard]] bool is_stuck() const noexcept
	{
		return (steady_clock_now() - task_start_time_.load(std::memory_order_acquire)) > max_task_execution_time_;
	}

	[[nodiscard]] std::chrono::milliseconds stuck_duration() const noexcept
	{
		return steady_clock_now() - task_start_time_.load(std::memory_order_acquire);
	}

	void log(const std::string & what, const std::string & code_filename, unsigned int code_line) const
	{
		if (!logger_)
		{
			return;
		}
		std::string msg("{\"what\":\"");
		msg.append(what)
			.append("\",\"highway_name\":\"")
			.append(highway_name_)
			.append("\",\"code_filename\":\"")
			.append(code_filename)
			.append("\",\"code_line\":")
			.append(std::to_string(code_line))
			.append("}");

		logger_->log(std::move(msg));
	}

	const std::string highway_name_;
	const LoggerPtr logger_;
	const std::chrono::milliseconds max_task_execution_time_;
	std::atomic<std::uint32_t> global_run_id_{0};

	enum class WhatRunningNow
	{
		Sleep,
		MailBoxMessage,
		ReschedulableRunnable,
		FreeTimeCustomLogic,
		Stopped
	};
	std::atomic<WhatRunningNow> what_is_running_now_{WhatRunningNow::Sleep};
	// https://www.mail-archive.com/llvm-bugs@lists.llvm.org/msg18280.html
	// std::atomic<std::chrono::time_point<std::chrono::steady_clock>> task_start_time_;
	std::atomic<std::chrono::milliseconds> task_start_time_;
	MailBox<Runnable> mail_box_;
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
		std::string highway_name,
		LoggerPtr logger,
		std::chrono::milliseconds max_task_execution_time)
		: self_weak_{self_protector}
		, bundle_{std::move(highway_name), std::move(logger), max_task_execution_time}
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

	const std::atomic<std::uint32_t> & global_run_id()
	{
		return bundle_.global_run_id_;
	}

	// Должен обязательно вызываться для удаления
	// Хайвей держит shared_ptr на себя чтобы не был случайно вызван деструктор до
	// того момента когда рабочий поток остановлен.
	virtual void destroy() = 0;

	// Запуск самопроверки хайвея
	// если вернуть false, значит хайвей больше не хочет чтобы его мониторили
	// (например если уже был вызван destroy())
	virtual bool self_check() = 0;

	// Проверка запущен ли код на данном хайвее
	virtual bool current_execution_on_this_highway() = 0;

	/*
	 Установка максимального количества сообщений(==холдеров) которые могут обрабатываться хайвеем в моменте
	 В холдеры помещаются объекты и после этого можно их поместить в mail_box_ - это позволяет
	 контроллировать расход оперативной памяти.
	 Рекомендуется устанавливать в соответствии с ограничениями на RAM - если количество сообщений
	 может бесконечно расти, то это может забить любой объём RAM (например если потребитель сообщений
	 обрабатывает сообщения медленнее чем производители сообщений их производят)
	*/
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
			self_weak_,
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
};

/*
	Обобщающая функция запуска Runnable на хайвее
	@highway - экзекутор на котором должен выполняться код колбэка
	@callback - колбэк обработчика публикации
	@send_may_fail - можно пропустить отправку если на хайвее подписчика закончились холдеры сообщений
	@filename, @line - координаты кода для отладки
*/
template <typename R>
void execute(
	std::shared_ptr<IHighWay> highway,
	R && callback,
	bool send_may_fail = false,
	std::string filename = __FILE__,
	unsigned int line = __LINE__)
{
	struct RunnableHolder
	{
		RunnableHolder(R && r)
			: r_{std::move(r)}
		{
		}

		void operator()(
			[[maybe_unused]] const std::atomic<std::uint32_t> & global_run_id,
			[[maybe_unused]] const std::uint32_t your_run_id)
		{
			if constexpr (std::is_invocable_v<R, const std::atomic<std::uint32_t> &, const std::uint32_t>)
			{
				r_(global_run_id, your_run_id);
			}
			else
			{
				r_();
			}
		}

		R r_;
	};

	auto runnable = hi::Runnable::create(RunnableHolder{std::move(callback)}, std::move(filename), line);
	if (send_may_fail)
	{
		highway->mailbox()->send_may_fail(std::move(runnable));
	}
	else
	{
		highway->mailbox()->send_may_blocked(std::move(runnable));
	}
}

} // namespace hi

#endif // IHIGHWAY_H
