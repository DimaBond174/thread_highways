#ifndef THREADS_HIGHWAYS_TOOLS_BUFFER_TRUNCATOR_H
#define THREADS_HIGHWAYS_TOOLS_BUFFER_TRUNCATOR_H

#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/result_code.h>

#include <cstdint>
#include <cstdlib> // std::malloc
#include <cstring> // memcpy
#include <functional>

namespace hi
{

// Используется для чистки буферов и memmap на месте
//  - когда есть данные которые необходимо убрать из буфера(memmap)
// и эти данные могут быть расположены очень часто, нужна дефрагментация на месте.
class BufferTruncator
{
public:
	BufferTruncator(
		std::int32_t size,
		std::function<result_t(const char * /*buf*/, std::int32_t /*buf_size*/)> flush_cb)
		: buf_{static_cast<char *>(std::malloc(static_cast<std::size_t>(size)))}
		, flush_cb_{std::move(flush_cb)}
		, buf_size_{size}
	{
	}

	~BufferTruncator()
	{
		std::free(buf_);
	}

	result_t copy(const char * from, std::int32_t from_size)
	{
		if (!buf_)
			return eNoMemory;
		HI_ASSERT(from_size > 0 && from);
		// if (from_size <= 0 || !from) return eWrongParams;
		while (from_size)
		{
			std::int32_t buf_size = buf_size_ - buf_offset_;
			if (buf_size > from_size)
				buf_size = from_size;
			char * to = buf_ + buf_offset_;
			std::memcpy(to, from, buf_size);
			buf_offset_ += buf_size;
			from += buf_size;
			from_size -= buf_size;
			if (buf_offset_ == buf_size_)
			{
				RETURN_CODE_IF_FAIL(flush());
			}
		}
		return ok;
	}

	// Выгрузка накопленного
	result_t flush()
	{
		auto res = flush_cb_(buf_, buf_offset_);
		if (res < 0)
			return res;
		buf_offset_ = 0;
		return res;
	}

private:
	char * buf_;
	std::function<result_t(const char * /*buf*/, std::int32_t /*buf_size*/)> flush_cb_;
	std::int32_t buf_size_;
	std::int32_t buf_offset_{0};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_BUFFER_TRUNCATOR_H
