/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_UPLOADER_TO_FD_H
#define THREADS_HIGHWAYS_DSON_UPLOADER_TO_FD_H

#include <thread_highways/dson/detail/i_uploader.h>
#include <thread_highways/dson/detail/obj_view.h>
#include <thread_highways/tools/fd_write_read.h>
#include <thread_highways/tools/result_code.h>

#include <memory>

namespace hi
{
namespace dson
{

class UploaderToFD : public detail::DefaultUploader
{
public:
	result_t start_upload(const std::int32_t fd, IObjView & obj)
	{
		reset_uploader();
		finish_offset_ = obj.all_size();
		return upload_step_impl(fd, obj);
	}

	/*
	 * @brief upload
	 * Неблокирующая POSIX запись в fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 */
	result_t upload_step(const std::int32_t fd, IObjView & obj)
	{
		return upload_step_impl(fd, obj);
	} // upload_step

	// Блокирующая запись объекта целиком
	result_t upload(const std::int32_t fd, IObjView & obj)
	{
		auto res = start_upload(fd, obj);
		while (okInProcess == res)
		{
			res = upload_step_impl(fd, obj);
		}
		return res;
	}

protected:
	result_t upload_step_impl(const std::int32_t fd, IObjView & obj)
	{
		switch (state_)
		{
		case State::Error:
			return eFail;
		case State::Finished:
			return okReady;
		case State::StartPosition:
			if (obj.upload_to(0, *this) < 0)
			{
				state_ = State::Error;
				return eFail;
			}
			// Выше установил загрузку хидера, и сразу выгружаю из локального буфера
			return upload_step_impl(fd, obj);
		case State::Next:
			return eFail;
		case State::UploadingFromLocalBuf:
			return upload_impl(fd, local_buf_, obj);
		case State::UploadingFromRemoteBuf:
			return upload_impl(fd, remote_chunk_, obj);
		} // switch
		return eFail;
	} // upload_step_impl

private:
	result_t upload_impl(const std::int32_t fd, const char * buf, IObjView & obj)
	{
		while (chunk_offset_ < remote_chunk_size_)
		{
			auto to_upload = remote_chunk_size_ - chunk_offset_;
			const auto writed = write_to_fd(fd, buf + chunk_offset_, to_upload);
			if (writed < 0)
			{
				state_ = State::Error;
				return eFail;
			}
			if (writed == 0)
			{
				return okInProcess;
			}
			chunk_offset_ += writed;
			// cur_offset_ += writed; //сейчас выгружаю из чанка, а cur_offset уже был проставлен
			if (chunk_offset_ == remote_chunk_size_)
			{
				if (cur_offset_ == finish_offset_)
				{
					state_ = State::Finished;
					return okReady;
				}
				// Загрузили объект, двигаемся на начало следующего объекта
				start_offset_ += remote_chunk_size_;
				chunk_offset_ = 0;
				return set_next_chunk(obj);
			}
		} // while
		throw hi::Exception(
			"Error chunk_offset_=" + std::to_string(chunk_offset_)
				+ ", remote_chunk_size_=" + std::to_string(remote_chunk_size_),
			__FILE__,
			__LINE__);
		return eFail;
	}

	result_t set_next_chunk(IObjView & obj)
	{
		// if (obj.upload_to(cur_offset_ - start_offset_, *this) < 0)
		//  {
		//         state_ = State::Error;
		//         return eFail;
		//   }
		//   if (remote_chunk_size_ + cur_offset_ > finish_offset_)
		//   {
		//     state_ = State::Error;
		//      return eFail;
		//   }
		//     return okInProcess;
		auto res = obj.upload_to(cur_offset_, *this);
		if (res < 0)
			state_ = State::Error;
		return res;
	}
};

// С захватом объекта (например для асинхронной выгрузки в сеть)
class UploaderToFDholder : public UploaderToFD
{
public:
	void set(std::unique_ptr<IObjView> obj)
	{
		reset_uploader();
		obj_ = std::move(obj);
		finish_offset_ = obj_->all_size();
	}

	void set(IObjView * obj)
	{
		set(std::unique_ptr<IObjView>(obj));
	}

	/*
	 * @brief upload
	 * POSIX запись в fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 */
	result_t upload_step(const std::int32_t fd)
	{
		return upload_step_impl(fd, *obj_);
	}

private:
	std::unique_ptr<IObjView> obj_;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_UPLOADER_TO_FD_H
