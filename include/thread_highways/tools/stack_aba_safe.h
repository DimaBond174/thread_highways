/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_STACK_ABA_SAFE_H
#define THREADS_HIGHWAYS_TOOLS_STACK_ABA_SAFE_H

#include <atomic>
#include <cassert>
#include <cstdint>
#include <stack>
#include <vector>

namespace hi
{

struct AbaSafePointer
{
	std::uint32_t array_id_{0};
	std::uint32_t operation_id_{0}; // protector against ABA
};
inline constexpr AbaSafePointer aba_safe_pointer_null{0, 0};
inline bool operator==(const AbaSafePointer & lh, const AbaSafePointer & rh)
{
	return lh.array_id_ == rh.array_id_ && lh.operation_id_ == rh.operation_id_;
}
inline bool operator!=(const AbaSafePointer & lh, const AbaSafePointer & rh)
{
	return !(lh == rh);
}

template <typename T>
struct AbaSafeHolder
{
	AbaSafeHolder()
	{
	}

	AbaSafeHolder(T && t)
		: t_{std::move(t)}
	{
	}
	AbaSafeHolder(const AbaSafeHolder &) = delete;
	AbaSafeHolder(AbaSafeHolder && rhs)
	{
		t_ = std::move(rhs.t_);
		next_in_stack_ = rhs.next_in_stack_;
		rhs.next_in_stack_ = aba_safe_pointer_null;
	}

	AbaSafeHolder & operator=(const AbaSafeHolder &) = delete;
	AbaSafeHolder & operator=(AbaSafeHolder && rhs)
	{
		if (this == &rhs)
			return *this;
		t_ = std::move(rhs.t_);
		next_in_stack_ = rhs.next_in_stack_;
		rhs.next_in_stack_ = aba_safe_pointer_null;
		return *this;
	}

	T t_{};
	AbaSafePointer next_in_stack_{aba_safe_pointer_null};
	AbaSafePointer this_{aba_safe_pointer_null};
};

template <typename T>
class ThreadAbaSafeStack
{
public:
	ThreadAbaSafeStack(std::uint32_t size)
	{
		pool_.resize(size);
		for (std::uint32_t i = 1u; i < size; ++i)
		{
			pool_[i].this_.array_id_ = i;
		}
	}

	bool empty()
	{
		return head_.load(std::memory_order_acquire) == aba_safe_pointer_null;
	}

	[[nodiscard]] AbaSafeHolder<T> * allocate_holder()
	{
		AbaSafeHolder<T> * re = pop_use_head(free_head_);
		if (re)
			return re;
		const auto size = pool_.size();
		if (size <= cur_allocation_id_.load(std::memory_order_acquire))
		{
			return nullptr;
		}
		const auto id = cur_allocation_id_.fetch_add(1u, std::memory_order_release);
		if (id < size)
			return &pool_[id];
		return nullptr;
	}

	void free_holder(AbaSafeHolder<T> & node)
	{
		push_use_head(free_head_, node);
	}

	void push(AbaSafeHolder<T> & node) noexcept
	{
		push_use_head(head_, node);
	}

	[[nodiscard]] AbaSafeHolder<T> * pop() noexcept
	{
		return pop_use_head(head_);
	}

	void move_to(std::stack<T> & to)
	{
		auto ptr = get_stack(head_);
		while (ptr)
		{
			to.emplace(std::move(ptr->t_));
			auto next = get_next(*ptr);
			free_holder(*ptr);
			ptr = next;
		}
	}

private:
	AbaSafeHolder<T> * get_next(AbaSafeHolder<T> & from)
	{
		if (from.this_ == aba_safe_pointer_null)
			return nullptr;
		if (from.next_in_stack_ == aba_safe_pointer_null)
			return nullptr;
		return &pool_[from.next_in_stack_.array_id_];
	}

	void push_use_head(std::atomic<AbaSafePointer> & head, AbaSafeHolder<T> & node) noexcept
	{
		assert(&pool_[0] < &node && &node <= &pool_[pool_.size() - 1]);
		++node.this_.operation_id_; // ABA protection

		// тут перетекает ABA protection из this_ в next_in_stack_
		node.next_in_stack_ = head.load(std::memory_order_relaxed);

		// https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange
		while (!head.compare_exchange_weak(
			node.next_in_stack_,
			node.this_,
			std::memory_order_release,
			std::memory_order_relaxed))
		{
		}
	}

	[[nodiscard]] AbaSafeHolder<T> * pop_use_head(std::atomic<AbaSafePointer> & head) noexcept
	{
		AbaSafePointer ptr = head.load(std::memory_order_acquire);
		/*
		 * Here, if it freezes and another thread manages to destroy re,
		 *  then there will be SIGSEGV on the call to re->next_in_stack_.
		 * That's why I don't deallocate holders.
		 */
		while (aba_safe_pointer_null != ptr && // exits if stack is empty
			   // if head_ == ptr, then change head_ to ptr->next_in_stack_
			   !head.compare_exchange_weak(
				   ptr,
				   pool_[ptr.array_id_].next_in_stack_,
				   std::memory_order_release,
				   std::memory_order_relaxed))
		{
		}
		if (aba_safe_pointer_null == ptr)
			return nullptr;
		// Ptr захвачен атомарно => только этот поток работает с ячейкой  ptr => могу менять ячейку
		AbaSafeHolder<T> * re = &pool_[ptr.array_id_];
		++re->this_.operation_id_; // ABA protection
		return re;
	}

	// Извлекаю весь стэк для однопоточного обхода
	[[nodiscard]] AbaSafeHolder<T> * get_stack(std::atomic<AbaSafePointer> & head) noexcept
	{
		AbaSafePointer ptr = head.exchange(aba_safe_pointer_null, std::memory_order_acq_rel);
		if (ptr == aba_safe_pointer_null)
			return nullptr;
		AbaSafeHolder<T> * re = &pool_[ptr.array_id_];
		++re->this_.operation_id_; // ABA protection
		return re;
	}

private:
	// Стэк
	std::atomic<AbaSafePointer> head_{aba_safe_pointer_null};
	// Пул освобождённых объектов
	std::atomic<AbaSafePointer> free_head_{aba_safe_pointer_null};
	// Пул новых объектов
	std::vector<AbaSafeHolder<T>> pool_;
	// Текущий указатель в пуле новых объектов
	std::atomic<std::size_t> cur_allocation_id_{1u};
}; // ThreadAbaSafeStack

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_STACK_ABA_SAFE_H
