/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_POSIX_MEM_MAP_PROXY_H
#define THREADS_HIGHWAYS_TOOLS_POSIX_MEM_MAP_PROXY_H

#include <thread_highways/tools/exception.h>
#include <thread_highways/tools/result_code.h>

#include <cstdint>
#include <cstring> // memcpy
#include <fcntl.h> // open
#include <functional>
#include <limits>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h> // fstat
#include <unistd.h> // close

namespace hi
{

// Доступ к файлу только в момент использования
// Для экономии ресурсов можно временно закрывать
// например когда в составе DsonFromFile ожидается длительное неиспользование
class MemMapProxy
{
public:
	MemMapProxy() = default;
	MemMapProxy(const MemMapProxy &) = delete;
	MemMapProxy(MemMapProxy && other)
	{
		move_from_other(std::move(other));
	}

	MemMapProxy & operator=(const MemMapProxy &) = delete;
	MemMapProxy & operator=(MemMapProxy && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		move_from_other(std::move(other));
		return *this;
	}
	~MemMapProxy()
	{
		close();
	}

	std::int32_t file_size()
	{
		return file_size_;
	}

	const std::string & file_path()
	{
		return path_;
	}

	std::int32_t window_offset()
	{
		return window_offset_;
	}

	std::int32_t window_size()
	{
		return window_size_;
	}

	void set_max_window_size(std::int32_t max_window_size)
	{
		if (max_window_size <= 0)
			return;
		max_window_size_ = max_window_size;
	}

	std::int32_t get_max_window_size()
	{
		return max_window_size_;
	}

	std::int32_t get_max_window_size_for_file()
	{
		return file_size_ < max_window_size_ ? file_size_ : max_window_size_;
	}

	result_t resize(std::int32_t new_size)
	{
		unmap();
		if (fd_ < 0)
		{
			RETURN_CODE_IF_FAIL(reopen_file());
		}
		if (file_size_ > new_size)
		{
			if (const auto res = ftruncate(fd_, new_size); res != 0)
			{
				throw Exception(
					"ftruncate fail, fd_=" + std::to_string(fd_) + ", new_size=" + std::to_string(new_size)
						+ ", res=" + std::to_string(res),
					__FILE__,
					__LINE__);
			}
			file_size_ = new_size;
		}
		else if (file_size_ < new_size)
		{
			file_size_ = new_size;
			set_file_size();
		}

		return reopen();
	}

	char * get_window(std::int32_t window_offset, std::int32_t window_size)
	{
		HI_ASSERT(window_offset >= 0 && window_size > 0);
		// if (window_offset < 0 || window_size <= 0) return nullptr;
		HI_ASSERT(file_size_ >= window_offset + window_size);
		// if (file_size_ < window_offset + window_size) return nullptr;
		if (!buf_ || window_offset_ > window_offset || (window_offset_ + window_size_ < window_offset + window_size))
		{
			if (memmap(window_offset, window_size) < 0)
			{
				return nullptr;
			}
		}
		return buf_ + (window_offset - window_offset_);
	}

	char * get_window(std::int32_t window_offset, std::int32_t target_window_size, std::int32_t & result_window_size)
	{
		HI_ASSERT(window_offset >= 0 && target_window_size > 0);
		// if (window_offset < 0 || target_window_size <= 0) return nullptr;
		HI_ASSERT(file_size_ >= window_offset + target_window_size);
		// if (file_size_ < window_offset + target_window_size) return nullptr;
		if (!buf_ || window_offset < window_offset_ // окно левее чем открытое
			|| (window_offset_ + window_size_ < window_offset + target_window_size)) // окно правее чем открытое
		{
			auto result = file_size_ - window_offset;
			if (result <= 0)
				return nullptr;
			if (result > target_window_size)
				result = target_window_size;
			if (result > max_window_size_)
				result = max_window_size_;
			if (memmap(window_offset, result) < 0)
			{
				return nullptr;
			}
			result_window_size = result;
		}
		else
		{
			result_window_size = target_window_size;
		}
		return buf_ + (window_offset - window_offset_);
	}

	char * get_window(char * cur, std::int32_t window_size)
	{
		auto diff = cur - buf_;
		if (file_size_ < window_offset_ + window_size)
			return nullptr;
		return get_window(window_offset_ + diff, window_size);
	}

	// Загрузка данных пользователя в файл
	result_t copy_in(std::int32_t offset, const char * chunk, std::int32_t chunk_size)
	{
		HI_ASSERT(chunk && chunk_size > 0);
		std::int32_t cur_chunk_size{0};
		while (chunk_size)
		{
			char * buf = get_window(offset, chunk_size, cur_chunk_size);
			HI_ASSERT(buf);
			//            if (!buf)
			//            {
			//                return eFailMemmap;
			//            }
			memcpy(buf, chunk, cur_chunk_size);
			chunk_size -= cur_chunk_size;
			chunk += cur_chunk_size;
			offset += cur_chunk_size;
		}
		return ok;
	}

	// Выгрузка данных в метод пользователя
	result_t copy_out(
		std::int32_t offset,
		std::int32_t size,
		const std::function<result_t(const char * /*from*/, std::int32_t /*from_size*/)> & target)
	{
		HI_ASSERT(size > 0 && offset >= 0);
		std::int32_t step_size = size > max_window_size_ ? max_window_size_ : size;
		while (size > 0)
		{
			if (size < step_size)
				step_size = size;
			char * buf = get_window(offset, step_size);
			HI_ASSERT(buf);
			// if (!buf) return eFailMemmap;
			auto res = target(buf, step_size);
			if (res < 0)
				return res;
			size -= step_size;
			offset += step_size;
		}
		return ok;
	}

	//    result_t copy_from(MemMapProxy& from, std::int32_t offset_from, std::int32_t size_from, std::int32_t
	//    offset_to)
	//    {
	//        if (size_from <= 0 || offset_from < 0 || offset_to < 0) return eWrongParams;
	//        std::int32_t step_size = size_from > max_window_size_? max_window_size_ : size_from;
	//        while (size_from > 0)
	//        {
	//            if (size_from < step_size) step_size = size_from;
	//            char* from_buf = from.get_window(offset_from, step_size);
	//            if (!from_buf) return eFailMemmap;
	//            char* to_buf = from.get_window(offset_to, step_size);
	//            if (!to_buf) return eFailMemmap;
	//            std::memcpy(to_buf, from_buf, step_size);
	//            size_from -= step_size;
	//            offset_from += step_size;
	//            offset_to += step_size;
	//        }
	//        return ok;
	//    }

	result_t reopen()
	{
		RETURN_CODE_IF_FAIL(reopen_file());
		return memmap(0, get_max_window_size_for_file());
	}

	result_t open(std::string path)
	{
		path_ = std::move(path);
		return reopen();
	}

	result_t create(std::string path, std::int32_t size = 0)
	{
		close();
		HI_ASSERT(size >= 0);
		path_ = std::move(path);
		fd_ = ::creat(path_.c_str(), S_IRUSR | S_IWUSR);
		if (fd_ < 0)
		{
			throw Exception("Fail to creat path_=" + path_, __FILE__, __LINE__);
		}
		file_size_ = 0; // O_TRUNC
		if (size == 0)
		{
			return okCreatedNew; // nothing to map
		}
		return resize(size);
	}

	void close()
	{
		if (fd_ < 0)
			return;
		unmap();
		::close(fd_);
		fd_ = -1;
	}

	const std::string & path()
	{
		return path_;
	}

private:
	void set_file_size()
	{
		/* установить размер выходного файла */
		if (::lseek(fd_, file_size_ - 1, SEEK_SET) == -1)
		{
			throw Exception(
				"lseek fail, fd_=" + std::to_string(fd_) + ", file_size_=" + std::to_string(file_size_),
				__FILE__,
				__LINE__);
		}

		if (::write(fd_, "", 1) != 1)
		{
			throw Exception("write fail, fd_=" + std::to_string(fd_), __FILE__, __LINE__);
		}
	}

	result_t get_file_size()
	{
		struct stat statbuf;
		if (auto res = ::fstat(fd_, &statbuf); res < 0)
		{
			throw Exception(
				"fstat fail, res=" + std::to_string(res) + ", fd_=" + std::to_string(fd_),
				__FILE__,
				__LINE__);
		}
		if (std::numeric_limits<std::int32_t>::max() < statbuf.st_size)
		{
			throw Exception("file too big, st_size=" + std::to_string(statbuf.st_size), __FILE__, __LINE__);
		}
		file_size_ = static_cast<std::int32_t>(statbuf.st_size);
		HI_ASSERT(file_size_ >= 0);
		if (file_size_ == 0)
			return okCreatedNew;
		return ok;
	}

	result_t memmap(std::int32_t offset, std::int32_t window_size)
	{
		if (window_size <= 0 || offset < 0 || offset > file_size_)
		{
			throw Exception(
				"WrongParams offset=" + std::to_string(offset) + ", window_size=" + std::to_string(window_size),
				__FILE__,
				__LINE__);
		}

		if (window_size > file_size_)
			window_size = file_size_;
		if (!file_size_)
			return okCreatedNew;
		void * re{nullptr};
		if ((re = mmap(0, window_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, offset)) == MAP_FAILED)
		{
			throw Exception(
				"FailMemmap offset=" + std::to_string(offset) + ", window_size=" + std::to_string(window_size)
					+ ", fd_=" + std::to_string(fd_),
				__FILE__,
				__LINE__);
		}
		buf_ = static_cast<char *>(re);
		window_size_ = window_size;
		window_offset_ = offset;
		return ok;
	}

protected:
	void move_from_other(MemMapProxy && other)
	{
		path_ = std::move(other.path_);
		file_size_ = other.file_size_;
		window_size_ = other.window_size_;
		window_offset_ = other.window_offset_;
		fd_ = other.fd_;
		buf_ = other.buf_;
		other.fd_ = -1;
		other.buf_ = nullptr;
	}

	void unmap()
	{
		if (buf_)
		{
			munmap(buf_, static_cast<size_t>(window_size_));
			buf_ = nullptr;
		}
	}

	result_t reopen_file()
	{
		close();
		fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd_ < 0)
		{
			throw Exception("Fail to open file: " + path_, __FILE__, __LINE__);
		}
		return get_file_size();
	}

private:
	std::string path_;
	std::int32_t file_size_{0};
	std::int32_t window_size_{0};
	std::int32_t window_offset_{0};
	std::int32_t max_window_size_{1024 * 1024};
	int fd_{-1};
	char * buf_{nullptr};
};

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_POSIX_MEM_MAP_PROXY_H
