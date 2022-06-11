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
	MailBox(InternalLogger logger)
		: logger_{std::move(logger)}
	{
	}

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

	/**
	 * @brief wait_for_new_messages
	 * Waiting for new messages using a semaphore.
	 */
	void wait_for_new_messages()
	{
		if (!messages_stack_.access_stack())
		{
			messages_stack_semaphore_.wait();
		}
	}

	/**
	 * @brief wait_for_new_messages
	 * Waiting for new messages with a semaphore, waiting time is limited.
	 * @param ns - max wait time
	 */
	void wait_for_new_messages(std::chrono::nanoseconds ns)
	{
		if (!messages_stack_.access_stack())
		{
			messages_stack_semaphore_.wait_for(ns);
		}
	}

	/**
	 * @brief load_new_messages
	 * Moving the stack of current messages
	 *  so that the message queue is in the correct order.
	 * @note typically this method is called from the main thread
	 * and then notifies the worker threads of the work to be done.
	 */
	void load_new_messages()
	{
		messages_stack_.move_to(work_queue_);
	}

	/**
	 * @brief signal_to_work_queue_semaphore
	 * Notifying worker threads that work has appeared.
	 * @param count - the number of workers who need to open the semaphore.
	 */
	void signal_to_work_queue_semaphore(std::uint32_t count)
	{
		work_queue_semaphore_.signal(count);
	}

	/**
	 * @brief worker_wait_for_messages
	 * Setting a worker thread to wait on a semaphore.
	 */
	void worker_wait_for_messages()
	{
		work_queue_semaphore_.wait();
	}

	/**
	 * @brief pop_message
	 * Extracting one holder with a message.
	 * @return holder or nullptr
	 */
	[[nodiscard]] Holder<T> * pop_message()
	{
		return work_queue_.pop();
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
		empty_holders_stack_.push(holder);
		empty_holders_stack_semaphore_.signal();
	}

	/**
	 * @brief signal_to_all
	 * Remove all threads from waiting on semaphores.
	 * For example, when the highway is already stopping its work.
	 */
	void signal_to_all()
	{
		empty_holders_stack_semaphore_.signal_to_all();
		work_queue_semaphore_.signal_to_all();
		messages_stack_semaphore_.signal_to_all();
	}

	/**
	 * @brief destroy
	 * Preparing a mailbox for destruction
	 */
	void destroy()
	{
		empty_holders_stack_semaphore_.destroy();
		work_queue_semaphore_.destroy();
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
		Holder<T> * holder = empty_holders_stack_.pop();
		if (holder)
		{
			holder->t_ = std::move(t);
		}
		else
		{
			if (capacity_.load(std::memory_order_relaxed) < allocated_holders_.load(std::memory_order_relaxed))
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

	/**
	 * @brief send_may_blocked
	 * Passing the message object to the mailbox for storage.
	 * If the holders are over, then it will block on the semaphore
	 *  and will wait for the holders to become free.
	 * @param t - message object
	 */
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

	// Filled pending holders
	ThreadSafeStack<Holder<T>> messages_stack_;
	Semaphore messages_stack_semaphore_;

	// Pool of free holders
	ThreadSafeStack<Holder<T>> empty_holders_stack_;
	Semaphore empty_holders_stack_semaphore_;

	// Holder allocation limiter
	std::atomic<std::uint32_t> capacity_{1024};
	std::atomic<std::uint32_t> allocated_holders_{0};

	// Unrolled stack - so the messages are now in the correct order
	ThreadSafeStack<Holder<T>> work_queue_;
	Semaphore work_queue_semaphore_;
};

} // namespace hi

#endif // THREAD_MAIL_BOX_WITH_CAPACITY_AND_SEMA_H
