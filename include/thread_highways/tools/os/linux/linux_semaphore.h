#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <semaphore.h>
#include <errno.h>

namespace hi
{

class Semaphore
{
public:
	Semaphore(unsigned int initial_count = 0)
	{
		sem_init(&m_sema_, 0, initial_count);
	}

	~Semaphore()
	{
		// https://www.opennet.ru/man.shtml?topic=sem_destroy
		// It is safe to destroy an initialised semaphore upon which no threads are currently blocked.
		//  The effect of destroying a semaphore upon which other threads are currently blocked is undefined.
		// Т.е. выключить ожидаторы разрушением семафора - это не вариант
		sem_destroy(&m_sema_);
	}

	void destroy()
	{
		keep_run_.store(false, std::memory_order_release);
		int sval{-1};
		while (sem_getvalue(&m_sema_, &sval) >= 0 && sval <= 0)
		{
			sem_post(&m_sema_);
		}
	}

	bool wait()
	{
		return wait_for(std::chrono::nanoseconds{100000000});
	}

	bool wait_for(std::chrono::nanoseconds ns)
	{
		if (!keep_run_.load(std::memory_order_acquire))
		{
			return false;
		}
		// The basic POSIX semaphore does not support a block time, but rather a deadline.
		timespec deadline;

		clock_gettime(CLOCK_REALTIME, &deadline);
		const auto ns_count = ns.count() + deadline.tv_nsec;
		deadline.tv_sec += static_cast<decltype(deadline.tv_sec)>(ns_count / 1000000000);
		deadline.tv_nsec = static_cast<decltype(deadline.tv_nsec)>(ns_count % 1000000000);

		const auto rc = sem_timedwait(&m_sema_, &deadline);
		switch (rc)
		{
		case EINTR:
		case ETIMEDOUT:
		case 0:
			return keep_run_.load(std::memory_order_acquire);
		default:
			return false;
		}
	}

	// Если нужен true|false семафор - то не постим если уже запостили
	void signal_keep_one()
	{
		if (!keep_run_.load(std::memory_order_acquire))
		{
			return;
		}
		int sval{-1};
		if (sem_getvalue(&m_sema_, &sval) < 0)
		{ // невалидный семафор
			return;
		}
		if (sval < 2)
		{
			sem_post(&m_sema_);
		}
	}

	void signal()
	{
		if (keep_run_.load(std::memory_order_acquire))
		{
			sem_post(&m_sema_);
		}
	}

	void signal(std::uint32_t count)
	{
		while (keep_run_.load(std::memory_order_acquire) && count--)
		{
			sem_post(&m_sema_);
		}
	}

	void signal_to_all()
	{
		while (keep_run_.load(std::memory_order_acquire))
		{
			int sval{-1};
			if (sem_getvalue(&m_sema_, &sval) < 0 || sval > 0)
			{
				return;
			}
			sem_post(&m_sema_);
		}
	}

private:
	sem_t m_sema_;
	std::atomic_bool keep_run_{true};
	Semaphore(const Semaphore & other) = delete;
	Semaphore & operator=(const Semaphore & other) = delete;
};

} // namespace hi

#endif // SEMAPHORE_H
