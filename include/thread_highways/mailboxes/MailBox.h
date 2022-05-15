/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREAD_MAIL_BOX_WITH_CAPACITY_AND_SEMA_H
#define THREAD_MAIL_BOX_WITH_CAPACITY_AND_SEMA_H

#include <thread_highways/tools/logger.h>
#include <thread_highways/tools/os/system_switch.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace hi
{

//! Потокобезопасный безмьютоксный почтовый ящик
//! с котролем общего количество холдеров в работе
//! для работы когда отправителей много, а получатель один.
//! За счёт ограничения количества холдеров не может получиться так
//! что из-за медленной работы потребителя поставщики переполняют память
//!  - поставщики вынужденно будут ждать когда потребитель освободит холдер.
//!
//! Предполагаемая схема использования в цикле рабочего потока:
//!
//!	// цикл рабочего потока
//! 	while (!prefetch_done_.load())
//!	{
//!		// цикл обработки поступивших сообщений
//!		while (auto fun = thread_mail_box_.pop())
//!		{
//!			(*fun)();
//!		}
//!
//!		// в этом месте рабочий цикл может сделать
//!		// дополнительную полезную работу
//!		// prefetch_tasks_.poll();
//!
//!		// проверка появились ли новые сообщения
//!		if (thread_mail_box_.exists_messages())
//!		{
//!			// если новые сообщения появились, то идём их обрабатывать
//!			continue;
//!		}
template <typename T>
class MailBox
{
public:
	MailBox(InternalLogger logger)
		: logger_{std::move(logger)}
	{
	}

	void set_capacity(std::uint32_t capacity)
	{
		capacity_.store(capacity, std::memory_order_release);
	}

	void wait_for_new_messages()
	{
		if (!messages_stack_.access_stack())
		{
			messages_stack_semaphore_.wait();
		}
	}

	void wait_for_new_messages(std::chrono::nanoseconds ns)
	{
		if (!messages_stack_.access_stack())
		{
			messages_stack_semaphore_.wait_for(ns);
		}
	}

	void load_new_messages()
	{
		messages_stack_.move_to(work_queue_);
	}

	void signal_to_work_queue_semaphore(std::uint32_t count)
	{
		work_queue_semaphore_.signal(count);
	}

	void worker_wait_for_messages()
	{
		work_queue_semaphore_.wait();
	}

	[[nodiscard]] Holder<T> * pop_message()
	{
		return work_queue_.pop();
	}

	void free_holder(Holder<T> * holder)
	{
		empty_holders_stack_.push(holder);
		empty_holders_stack_semaphore_.signal();
	}

	void signal_to_all()
	{
		empty_holders_stack_semaphore_.signal_to_all();
		work_queue_semaphore_.signal_to_all();
		messages_stack_semaphore_.signal_to_all();
	}

	void destroy()
	{
		empty_holders_stack_semaphore_.destroy();
		work_queue_semaphore_.destroy();
		messages_stack_semaphore_.destroy();
	}

public: // IMailBoxSendHere
	//! передача объекта сообщения на хранение в контейнер
	//! потокобезопасно, без блокировки но если нехватка холдеров
	//! то может не доставить
	//! @param t - объект
	//! @return получилось ли доставить
	bool send_may_fail(T && t)
	{
		Holder<T> * holder = empty_holders_stack_.pop();
		if (holder)
		{
			holder->t_ = std::move(t);
		}
		else
		{
			if (capacity_.load(std::memory_order_relaxed) <= allocated_holders_.load(std::memory_order_relaxed))
			{
				logger_("MailBox: no more holders", __FILE__, __LINE__);
				return false;
			}
			++allocated_holders_;
			holder = new Holder<T>{std::move(t)};
		}

		messages_stack_.push(holder);
		messages_stack_semaphore_.signal_keep_one();
		return true;
	}

	void send_may_blocked(T && t)
	{
		Holder<T> * holder = empty_holders_stack_.pop();
		if (holder)
		{
			holder->t_ = std::move(t);
		}
		else
		{
			if (capacity_.load(std::memory_order_relaxed) > allocated_holders_.load(std::memory_order_relaxed))
			{
				++allocated_holders_;
				holder = new Holder{std::move(t)};
			}
			else
			{
				while (!holder && empty_holders_stack_semaphore_.wait())
				{
					holder = empty_holders_stack_.pop();
				}
				if (holder)
				{
					holder->t_ = std::move(t);
				}
			}
		}

		if (holder)
		{
			messages_stack_.push(holder);
			messages_stack_semaphore_.signal_keep_one();
		}
	}

private:
	const InternalLogger logger_;

	// Заполненные ожидающие обработки холдеры
	ThreadSafeStack<Holder<T>> messages_stack_;
	Semaphore messages_stack_semaphore_;

	// Пул свободных холдеров
	ThreadSafeStack<Holder<T>> empty_holders_stack_;
	Semaphore empty_holders_stack_semaphore_;

	// Ограничитель аллокаций холдеров
	std::atomic<std::uint32_t> capacity_{1024};
	std::atomic<std::uint32_t> allocated_holders_{0};

	// развёрнутый стэк - так что сообщения теперь в правильном порядке.
	ThreadSafeStack<Holder<T>> work_queue_;
	Semaphore work_queue_semaphore_;
};

} // namespace hi

#endif // THREAD_MAIL_BOX_WITH_CAPACITY_AND_SEMA_H
