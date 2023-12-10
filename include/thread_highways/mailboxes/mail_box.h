/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_H
#define THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_H

#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/semaphore.h>
#include <thread_highways/tools/stack.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace hi
{

/**
 * @brief MailBox
 * Thread-safe mutexless mailbox with control over the total number of holders.
 * Holder is a container for your message to be added to the message queue.
 *
 * Usage example: https://github.com/DimaBond174/thread_highways/blob/main/include/thread_highways/highways/IHighWay.h
 */
template <typename T>
class MailBox
{
public:
	/**
	 * @brief set_capacity
	 * Setting the maximum number of holders in operation.
	 * @param capacity - how much holders
	 * @note the number of holders that have already been created does not decrease
	 */
	void set_capacity(const std::uint32_t capacity)
	{
		capacity_.store(capacity, std::memory_order_release);
	}

	void move_to(SingleThreadStack<Holder<T>> & work_queue, std::chrono::nanoseconds max_wait)
	{
		if (!messages_stack_.access_stack())
		{
			messages_stack_semaphore_.wait_for(max_wait);
		}
		messages_stack_.move_to(work_queue);
	}

	void move_to_no_wait(SingleThreadStack<Holder<T>> & work_queue)
	{
		messages_stack_.move_to(work_queue);
	}

	/**
	 * @brief pop_message
	 * Extracting one holder with a message.
	 * Do not use it with move_to();
	 * @return holder or nullptr
	 */
	[[nodiscard]] Holder<T> * pop_message()
	{
		Holder<T> * re = work_queue_.pop();
		while (!re && keep_execution_.load(std::memory_order_acquire))
		{
			if (!messages_stack_.access_stack())
			{
				messages_stack_semaphore_.wait();
			}
			messages_stack_.move_to(work_queue_);
			re = work_queue_.pop();
		}
		return re;
	}

	/**
	 * @brief pop_message
	 * Extracting one holder with a message.
	 * Do not use it with move_to();
	 * @return holder or nullptr
	 * @note not wait if empty
	 */
	[[nodiscard]] Holder<T> * pop_message_no_wait()
	{
		Holder<T> * re = work_queue_.pop();
		if (!re)
		{
			messages_stack_.move_to(work_queue_);
			re = work_queue_.pop();
		}
		return re;
	}

	/**
	 * @brief free_holder
	 * Returning a holder to the pool of released holders.
	 * @param holder
	 * @note for those who are waiting for the release
	 * of holders on the semaphore gives a signal
	 */
	void free_holder(Holder<T> * holder)
	{
		holder->t_.clear();
		empty_holders_stack_.push(holder);
		empty_holders_stack_semaphore_.signal();
	}

	/**
	 * @brief destroy
	 * Preparing a mailbox for destruction
	 */
	void destroy()
	{
		keep_execution_.store(false, std::memory_order_release);
		empty_holders_stack_semaphore_.destroy();
		messages_stack_semaphore_.destroy();
	}

public: // IMailBoxSendHere
	/**
	 * @brief send_may_fail
	 * Passing the message object to the mailbox for storage.
	 * Can ignore if holders run out.
	 * @param t - message object
	 * @return true if the send was successful
	 */
	bool send_may_fail(T && t)
	{
		Holder<T> * holder = aba_safe_get_free_holder();
		if (!holder)
			return false; // may_fail
		holder->t_ = std::move(t);

		messages_stack_.push(holder);
		messages_stack_semaphore_.signal_keep_one();
		return true;
	}

	/**
	 * @brief send_may_blocked
	 * Passing the message object to the mailbox for storage.
	 * If the holders are over, then it will block on the semaphore
	 *  and will wait for the holders to become free.
	 * @param t - message object
	 */
	void send_may_blocked(T && t)
	{
		Holder<T> * holder{nullptr};
		do
		{
			holder = aba_safe_get_free_holder();
			if (holder)
				break;
			empty_holders_stack_semaphore_.wait();
		}
		while (keep_execution_.load(std::memory_order_relaxed));

		if (!holder)
		{
			return; // keep_execution_ был сброшен
		}

		holder->t_ = std::move(t);
		messages_stack_.push(holder);
		messages_stack_semaphore_.signal_keep_one();
	}

private:
	Holder<T> * aba_safe_get_free_holder()
	{
		// Преимущество отдаётся аллокации новых холдеров чтобы получить capacity объём
		// в круге своего использования защищающий от ABA. Это даст замедление в начале работы
		// так как будет capacity_ аллокаций холдеров, но дальше холдеры будут переиспользоваться
		// и почтовый ящик выйдет на рабочую скорость (== есть эффект прогрева двигателя)
		if (capacity_.load(std::memory_order_relaxed) > allocated_holders_.load(std::memory_order_relaxed))
		{
			++allocated_holders_;
			return new Holder<T>{};
		}

		// Очередь обеспечивает защиту от ABA за счёт того что анализируемый на compare_exchange_weak
		// в конкурентном потоке указатель холдера после отработки в захватившем потоке вернётся
		// не в empty_holders_stack_, а в конец очереди empty_holders_queue_
		// и потом потребуется ещё один переход в messages_stack_. Этот двойной переход
		// с промежуточной capacity_ массой делает ABA практически невозможной.
		Holder<T> * holder = empty_holders_queue_.pop();
		if (holder)
			return holder;

		empty_holders_stack_.move_to(empty_holders_queue_);
		return empty_holders_queue_.pop();
	}

private:
	// Filled pending holders
	ThreadSafeStack<Holder<T>> messages_stack_;
	Semaphore messages_stack_semaphore_;

	// Pool of free holders
	ThreadSafeStack<Holder<T>> empty_holders_stack_;
	// Защита от ABA достигается за счёт того что холдер может вернуться в работу
	// только в порядке очереди использования capacity_ количества холдеров => для возникновения
	// ABA должно в момент выполнения compare_exchange_weak пройти через стэк capacity_ количества
	// => появляется инструмент снижения вероятности ABA через capacity_ параметр
	// (Сколько атомарных операций может пройти в момент фриза compare_exchange_weak?
	// В альтернативных защитах от ABA используется счётчик-идентификатор состояния холдера - по сути инструмент
	// аналогичный)
	ThreadSafeStack<Holder<T>> empty_holders_queue_;
	Semaphore empty_holders_stack_semaphore_;

	// Holder allocation limiter
	std::atomic<std::uint32_t> capacity_{1024};
	std::atomic<std::uint32_t> allocated_holders_{0};

	// Unrolled stack - so the messages are now in the correct order
	ThreadSafeStack<Holder<T>> work_queue_;
	std::atomic<bool> keep_execution_{true};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_H
