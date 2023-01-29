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

    void move_to(SingleThreadStack<Holder<T>>& work_queue, std::chrono::nanoseconds max_wait)
    {
        if (!messages_stack_.access_stack())
        {
            messages_stack_semaphore_.wait_for(max_wait);
        }
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
		Holder<T> * holder = empty_holders_stack_.pop();
		if (holder)
		{
			holder->t_ = std::move(t);
		}
		else
		{
			if (capacity_.load(std::memory_order_relaxed) < allocated_holders_.load(std::memory_order_relaxed))
			{
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
    std::atomic<bool> keep_execution_{true};
};

template <typename T>
struct IMailBoxSendHere
{
    virtual ~IMailBoxSendHere() = default;

    // Multi-threaded access
    virtual bool send_may_fail(T && t) = 0;
    // Если вернул false, значит почтового ящика больше нет и слать снова незачем
    virtual bool send_may_blocked(T && t) = 0;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_MAILBOXES_MAIL_BOX_H
