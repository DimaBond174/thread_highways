/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_STACK_H
#define THREADS_HIGHWAYS_TOOLS_STACK_H

#include <atomic>
#include <cstdint>

namespace hi
{

template <typename T>
struct Holder
{
	Holder(T && t)
		: t_{std::move(t)}
	{
	}
	Holder(const Holder &) = delete;
	Holder(Holder &&) = delete;

	Holder & operator=(const Holder &) = delete;
	Holder & operator=(Holder &&) = delete;

	T t_;
	Holder * next_in_stack_{nullptr};
};

template <typename Holder>
class SingleThreadStack
{
public:
	[[nodiscard]] bool empty() const noexcept
	{
		return !head_;
	}

	[[nodiscard]] bool not_empty() const noexcept
	{
		return !!head_;
	}

	void push(Holder * node) noexcept
	{
		if (!node)
			return;
		node->next_in_stack_ = head_;
		head_ = node;
	}

	[[nodiscard]] Holder * pop() noexcept
	{
		if (head_)
		{
			Holder * re = head_;
			head_ = re->next_in_stack_;
			return re;
		}
		return nullptr;
	}

	void swap(SingleThreadStack & other) noexcept
	{
		Holder * re = head_;
		head_ = other.head_;
		other.head_ = re;
		return;
	}

	void set_stack(Holder * head) noexcept
	{
		head_ = head;
	}

	[[nodiscard]] Holder * swap(Holder * new_head) noexcept
	{
		Holder * re = head_;
		head_ = new_head;
		return re;
	}

	[[nodiscard]] Holder * get_first() const noexcept
	{
		return head_;
	}

	void clear()
	{
		Holder * first = swap(nullptr);
		while (first)
		{
			Holder * next = first->next_in_stack_;
			delete first;
			first = next;
		}
	}

	~SingleThreadStack()
	{
		clear();
	}

private:
	Holder * head_{nullptr};
}; // SingleThreadStack

template <typename Holder>
class SingleThreadStackWithCounter
{
public:
	[[nodiscard]] bool not_empty() const noexcept
	{
		return !!head_;
	}
	[[nodiscard]] uint32_t size() const noexcept
	{
		return size_;
	}

	void push(Holder * node) noexcept
	{
		if (!node)
			return;
		node->next_in_stack_ = head_;
		head_ = node;
		++size_;
	}

	[[nodiscard]] Holder * pop() noexcept
	{
		if (head_)
		{
			Holder * re = head_;
			head_ = re->next_in_stack_;
			--size_;
			return re;
		}
		return nullptr;
	}

	[[nodiscard]] Holder * get_first() const noexcept
	{
		return head_;
	}

	void clear()
	{
		Holder * first = head_;
		while (first)
		{
			Holder * next = first->next_in_stack_;
			delete first;
			first = next;
		}
		head_ = nullptr;
		size_ = 0;
	}

	~SingleThreadStackWithCounter()
	{
		clear();
	}

private:
	Holder * head_{nullptr};
	uint32_t size_{0};
}; // SingleThreadStackWithCounter

template <typename Holder>
class ThreadSafeStack
{
public:
	~ThreadSafeStack()
	{
		clear();
	}

	void push(Holder * node) noexcept
	{
		if (!node)
			return;
		node->next_in_stack_ = head_.load(std::memory_order_relaxed);

		// https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange
		while (!head_.compare_exchange_weak(
			node->next_in_stack_,
			node,
			std::memory_order_release,
			std::memory_order_relaxed))
		{
		}
	}

	/**
	 * Tread safe pop an object from the stack.
	 * This method can only be used if the holders are not dallocated.
	 */
	[[nodiscard]] Holder * pop() noexcept
	{
		Holder * re = head_.load(std::memory_order_acquire);
		/*
		 * Here, if it freezes and another thread manages to destroy re,
		 *  then there will be SIGSEGV on the call to re->next_in_stack_.
		 * That's why I don't deallocate holders.
		 */
		while (
			re && // exits if stack is empty
			// if head_ == re, then change head_ to re->next_in_stack_
			!head_.compare_exchange_weak(re, re->next_in_stack_, std::memory_order_release, std::memory_order_relaxed))
		{
		}
		return re;
	}

	[[nodiscard]] Holder * access_stack() const noexcept
	{
		return head_.load(std::memory_order_acquire);
	}
	[[nodiscard]] Holder * get_stack() noexcept
	{
		return head_.exchange(nullptr, std::memory_order_acq_rel);
	}

	void move_to(SingleThreadStack<Holder> & to) noexcept
	{
		Holder * h = get_stack();
		while (h)
		{
			Holder * next = h->next_in_stack_;
			to.push(h);
			h = next;
		}
		return;
	}

	void fetch_from(SingleThreadStack<Holder> & from) noexcept
	{
		while (auto holder = from.pop())
		{
			push(holder);
		}
	}

	void move_to(ThreadSafeStack<Holder> & to) noexcept
	{
		Holder * h = get_stack();
		while (h)
		{
			Holder * next = h->next_in_stack_;
			to.push(h);
			h = next;
		}
		return;
	}

	[[nodiscard]] Holder * get_reverse() noexcept
	{
		SingleThreadStack<Holder> from;
		from.set_stack(head_.exchange(nullptr, std::memory_order_acq_rel));
		SingleThreadStack<Holder> to;
		while (Holder * ptr = from.pop())
		{
			to.push(ptr);
		}
		return to.swap(nullptr);
	}

	void get_reverse(SingleThreadStack<Holder> & to) noexcept
	{
		SingleThreadStack<Holder> from;
		from.set_stack(head_.exchange(nullptr, std::memory_order_acq_rel));
		while (Holder * ptr = from.pop())
		{
			to.push(ptr);
		}
		return;
	}

	[[nodiscard]] Holder * swap(Holder * new_head) noexcept
	{
		return head_.exchange(new_head, std::memory_order_acq_rel);
	}

	void clear()
	{
		Holder * first = get_stack();
		while (first)
		{
			Holder * next = first->next_in_stack_;
			delete first;
			first = next;
		}
	}

private:
	std::atomic<Holder *> head_{nullptr};
}; // ThreadSafeStack

template <typename Holder>
class ThreadSafeStackWithCounter
{
public:
	~ThreadSafeStackWithCounter()
	{
		Holder * first = head_.exchange(nullptr, std::memory_order_acq_rel);
		while (first)
		{
			Holder * next = first->next_in_stack_;
			delete first;
			first = next;
		}
	}

	std::uint32_t size()
	{
		return size_.load(std::memory_order_acquire);
	}

	void push(Holder * node) noexcept
	{
		if (!node)
			return;
		node->next_in_stack_ = head_.load(std::memory_order_relaxed);

		// https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange
		while (!head_.compare_exchange_weak(
			node->next_in_stack_,
			node,
			std::memory_order_release,
			std::memory_order_relaxed))
		{
		}
		++size_;
	}

	/**
	 * Tread safe pop an object from the stack.
	 * This method can only be used if the holders are not dallocated.
	 */
	[[nodiscard]] Holder * pop() noexcept
	{
		Holder * re = head_.load(std::memory_order_acquire);
		/*
		 * Here, if it freezes and another thread manages to destroy re,
		 *  then there will be SIGSEGV on the call to re->next_in_stack_.
		 * That's why I don't deallocate holders.
		 */
		while (
			re && // exits if stack is empty
			// if head_ == re, then change head_ to re->next_in_stack_
			!head_.compare_exchange_weak(re, re->next_in_stack_, std::memory_order_release, std::memory_order_relaxed))
		{
		}
		if (re)
		{
			--size_;
		}
		return re;
	}

	[[nodiscard]] Holder * access_stack() const noexcept
	{
		return head_.load(std::memory_order_acquire);
	}

private:
	std::atomic<Holder *> head_{nullptr};
	std::atomic<uint32_t> size_{0};
}; // ThreadSafeStackWithCounter

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_STACK_H
