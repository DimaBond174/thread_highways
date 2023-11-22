/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_BUF_UINT8_H
#define THREADS_HIGHWAYS_TOOLS_BUF_UINT8_H

#include <cassert>
#include <cstdint> // std::int32_t
#include <cstdlib> // std::malloc
#include <cstring> // std::memcpy

namespace hi
{

/*
	std::int32_t for OpenCV (https://docs.opencv.org/4.x/d3/d63/classcv_1_1Mat.html)
	unsigned char for strict aliasing (https://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule)
		and OpenCV ARGB/BGRA
		+ A pixel is not an integer. It's just a sequence of bytes. => ARGB/BGRA not depends on endianness
*/

class BufUCharView
{
public:
	using value_type = unsigned char;

	BufUCharView() = default;
	~BufUCharView()
	{
	}

	// View для чужого буфера
	BufUCharView(const value_type * data, const std::int32_t size)
		: buf_{data}
		, size_{size}
	{
	}

	// View для чужого буфера
	BufUCharView(const value_type * data, const value_type * end)
		: buf_{data}
		, size_{static_cast<std::int32_t>(end - data)}
	{
	}

	BufUCharView(const BufUCharView & rhs)
	{
		buf_ = rhs.buf_;
		size_ = rhs.size_;
	}

	BufUCharView(BufUCharView && rhs)
	{
		buf_ = rhs.buf_;
		size_ = rhs.size_;
	}

	BufUCharView & operator=(const BufUCharView & rhs)
	{
		buf_ = rhs.buf_;
		size_ = rhs.size_;
		return *this;
	}

	BufUCharView & operator=(BufUCharView && rhs)
	{
		buf_ = rhs.buf_;
		size_ = rhs.size_;
		return *this;
	}

	const value_type * data() const
	{
		return buf_;
	}

	std::int32_t size() const
	{
		return size_;
	}

	void init(const void * buf, std::int32_t size)
	{
		buf_ = static_cast<const value_type *>(buf);
		size_ = size;
	}

private:
	const value_type * buf_{nullptr};
	std::int32_t size_{0u};
};

class BufUChar
{
public:
	using value_type = unsigned char;

	BufUChar() = default;
	~BufUChar()
	{
		clear();
	}

	BufUChar(const BufUChar & rhs)
	{
		copy(rhs.buf_, rhs.size_);
	}

	BufUChar(BufUChar && rhs)
	{
		buf_ = rhs.buf_;
		rhs.buf_ = nullptr;

		size_ = rhs.size_;
		rhs.size_ = 0;

		reserved_ = rhs.reserved_;
		rhs.reserved_ = 0;
	}

	BufUChar & operator=(const BufUChar & rhs)
	{
		if (buf_ != rhs.buf_)
		{
			copy(rhs.buf_, rhs.size_);
		}
		return *this;
	}

	BufUChar & operator=(BufUChar && rhs)
	{
		if (buf_ != rhs.buf_)
		{
			clear();
			buf_ = rhs.buf_;
			rhs.buf_ = nullptr;

			size_ = rhs.size_;
			rhs.size_ = 0;

			reserved_ = rhs.reserved_;
			rhs.reserved_ = 0;
		}

		return *this;
	}

	BufUChar(const value_type * data, const std::int32_t size)
	{
		copy(data, size);
	}

	BufUChar(const BufUCharView & rhs)
	{
		copy(rhs.data(), rhs.size());
	}

	BufUChar(BufUCharView && rhs)
	{
		copy(rhs.data(), rhs.size());
	}

	BufUChar & operator=(const BufUCharView & rhs)
	{
		copy(rhs.data(), rhs.size());
		return *this;
	}

	BufUChar & operator=(BufUCharView && rhs)
	{
		copy(rhs.data(), rhs.size());
		return *this;
	}

	value_type * data()
	{
		return buf_;
	}

	const value_type * data() const
	{
		return buf_;
	}

	std::int32_t size() const
	{
		return size_;
	}

	// Взять ровно сколько нужно и установить размер сразу
	bool resize(std::int32_t alloc_size)
	{
		if (reserved_ != alloc_size)
		{
			clear();
			reserve(alloc_size);
		}
		size_ = alloc_size;
		return true;
	}

	// Взять больше памяти если нужно
	bool reserve(std::int32_t alloc_size)
	{
		if (reserved_ < alloc_size)
		{
			clear();
			buf_ = reinterpret_cast<value_type *>(std::malloc(alloc_size));
			if (!buf_)
				return false;
			reserved_ = alloc_size;
		}
		return true;
	}

	void set_size(std::int32_t size)
	{
		assert(reserved_ <= size);
		size_ = size;
	}

private:
	bool copy(const value_type * in_buf, const std::int32_t in_buf_size)
	{
		if (!in_buf_size || !in_buf)
			return false;
		if (reserved_ < in_buf_size)
		{
			if (!resize(in_buf_size))
				return false;
		}

		std::memcpy(buf_, in_buf, in_buf_size);
		size_ = in_buf_size;
		return true;
	}

	void clear()
	{
		if (reserved_ && buf_)
		{
			std::free(buf_);
		}
		buf_ = nullptr;
		size_ = 0;
		reserved_ = 0;
	}

private:
	value_type * buf_{nullptr};
	std::int32_t size_{0}; // Used
	std::int32_t reserved_{0}; // Allocated
};

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_BUF_UINT8_H
