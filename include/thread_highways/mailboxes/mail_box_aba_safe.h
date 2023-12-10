/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_ABA_SAFE_H
#define THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_ABA_SAFE_H

#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/semaphore.h>
#include <thread_highways/tools/stack_aba_safe.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace hi
{

/**
 * @brief MailBoxAbaSafe
 * Thread-safe mutexless mailbox with control over the total number of holders.
 * Holder is a container for your message to be added to the message queue.
 *
 */
template <typename T>
class MailBoxAbaSafe
{
public:
	MailBoxAbaSafe(std::uint32_t mail_box_capacity)
		: messages_stack_{mail_box_capacity}
		, work_queue_{mail_box_capacity}
	{
	}

	void move_stack(std::stack<T> & work_queue, std::chrono::nanoseconds max_wait)
	{
		if (messages_stack_.empty())
		{
			message_semaphore_.wait_for(max_wait);
		}
		messages_stack_.move_to(work_queue);
	}

	void move_stack_no_wait(std::stack<T> & work_queue)
	{
		messages_stack_.move_to(work_queue);
	}

	/**
	 * @brief pop_message
	 * Extracting one holder with a message.
	 * Do not use it with move_to();
	 * @return holder or nullptr
	 */
	[[nodiscard]] AbaSafeHolder<T> * pop_message()
	{
		AbaSafeHolder<T> * re = work_queue_.pop();
		while (!re && keep_execution_.load(std::memory_order_acquire))
		{
			if (messages_stack_.empty())
			{
				message_semaphore_.wait();
			}
			move_to_work_queue();
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
	[[nodiscard]] AbaSafeHolder<T> * pop_message_no_wait()
	{
		auto re = work_queue_.pop();
		if (!re)
		{
			move_to_work_queue();
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
	void free_messages_stack_holder(AbaSafeHolder<T> & holder)
	{
		messages_stack_.free_holder(holder);
		free_holder_semaphore_.signal();
	}

	void free_work_queue_holder(AbaSafeHolder<T> & holder)
	{
		work_queue_.free_holder(holder);
		free_holder_semaphore_.signal();
	}

	/**
	 * @brief destroy
	 * Preparing a mailbox for destruction
	 */
	void destroy()
	{
		keep_execution_.store(false, std::memory_order_release);
		message_semaphore_.destroy();
		free_holder_semaphore_.destroy();
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
		auto holder = messages_stack_.allocate_holder();
		if (!holder)
			return false;
		holder->t_ = std::move(t);
		messages_stack_.push(*holder);
		message_semaphore_.signal_keep_one();
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
		AbaSafeHolder<T> * holder{nullptr};
		do
		{
			holder = messages_stack_.allocate_holder();
			if (holder)
				break;
			free_holder_semaphore_.wait(); // std::memory_order_acquire
		}
		while (keep_execution_.load(std::memory_order_relaxed));

		if (!holder)
		{
			return; // keep_execution_ был сброшен
		}

		holder->t_ = std::move(t);
		messages_stack_.push(*holder);
		message_semaphore_.signal_keep_one();
	}

	void move_to_work_queue()
	{
		if (messages_stack_.empty())
			return;
		for (auto to_holder = work_queue_.allocate_holder(); to_holder; to_holder = work_queue_.allocate_holder())
		{
			auto from_holder = messages_stack_.pop();
			if (!from_holder)
			{
				work_queue_.free_holder(*to_holder);
				return;
			}
			to_holder->t_ = std::move(from_holder->t_);
			messages_stack_.free_holder(*from_holder);
			work_queue_.push(*to_holder);
		}
	}

private:
	// Filled pending holders
	ThreadAbaSafeStack<T> messages_stack_;
	// Сигнал про новое сообщение
	Semaphore message_semaphore_;
	// Сигнал про возможность принять новое сообщение
	Semaphore free_holder_semaphore_;

	// Holder allocation limiter
	std::atomic<std::uint32_t> capacity_{1024};
	std::atomic<std::uint32_t> allocated_holders_{0};

	// Unrolled stack - so the messages are now in the correct order
	ThreadAbaSafeStack<T> work_queue_;
	std::atomic<bool> keep_execution_{true};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_ABA_SAFE_H
