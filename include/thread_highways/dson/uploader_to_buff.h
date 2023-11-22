/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_UPLOADER_TO_BUFF_H
#define THREADS_HIGHWAYS_DSON_UPLOADER_TO_BUFF_H

#include <thread_highways/dson/detail/i_uploader.h>
#include <thread_highways/dson/detail/obj_view.h>
#include <thread_highways/tools/result_code.h>

#include <memory>

namespace hi
{
namespace dson
{

class UploaderToBuff : public detail::DefaultUploader
{
public:
	result_t start_upload(char *& buf, std::int32_t & buf_size, IObjView & obj)
	{
		reset_uploader();
		finish_offset_ = obj.all_size();
		return upload_step_impl(buf, buf_size, obj);
	}

	result_t upload_step(char *& buf, std::int32_t & buf_size, IObjView & obj)
	{
		return upload_step_impl(buf, buf_size, obj);
	}

protected:
	/*
	 * @brief upload
	 * запись в буффер (SharedMem/Memmap/..)
	 * @param buf курсор по буферу
	 * @param buf_size счётчик оставшегося объёма
	 * @return Result
	 */
	result_t upload_step_impl(char *& buf, std::int32_t & buf_size, IObjView & obj)
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
			return upload_step_impl(buf, buf_size, obj);
		case State::Next:
			return eFail;
		case State::UploadingFromLocalBuf:
			// return upload_impl(buf, buf_size, local_buf_, obj);
			{
				auto res = upload_impl(buf, buf_size, local_buf_, obj);
				if (res == okReady)
				{
					HI_ASSERT(cur_offset_ == finish_offset_);
				}
				return res;
			}
		case State::UploadingFromRemoteBuf:
			// return upload_impl(buf, buf_size, remote_chunk_, obj);
			{
				auto res = upload_impl(buf, buf_size, remote_chunk_, obj);
				if (res == okReady)
				{
					HI_ASSERT(cur_offset_ == finish_offset_);
				}
				return res;
			}
		} // switch
		return eFail;
	} // upload

private:
	result_t upload_impl(char *& external_buf, std::int32_t & buf_size, const char * buf, IObjView & obj)
	{
		auto to_upload = remote_chunk_size_ - chunk_offset_;
		const auto writed = std::min(buf_size, to_upload);
		std::memcpy(external_buf, buf + chunk_offset_, writed);
		external_buf += writed;
		chunk_offset_ += writed;
		// cur_offset_ += writed; сейчас выгружаю из чанка, а cur_offset уже был проставлен
		buf_size -= writed;
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
		return okInProcess;
	}

	result_t set_next_chunk(IObjView & obj)
	{
		//        // if (!obj.upload_to(cur_offset_ - start_offset_, *this)) так выгружаю опять хидер головного Dson
		//        if (obj.upload_to(cur_offset_, *this) < 0)
		//         {
		//                state_ = State::Error;
		//                return eFail;
		//          }
		////          if (remote_chunk_size_ - chunk_offset_ + cur_offset_ > finish_offset_)
		////          {
		////            state_ = State::Error;
		////                return eFail;
		////          }
		//            return okInProcess;
		auto res = obj.upload_to(cur_offset_, *this);
		if (res < 0)
			state_ = State::Error;
		return res;
	}
};

// С захватом объекта (например для асинхронной выгрузки в shared_mem)
class UploaderToBuffHolder : public UploaderToBuff
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
	 * запись в буффер (SharedMem/Memmap/..)
	 * @param buf курсор по буферу
	 * @param buf_size счётчик оставшегося объёма
	 * @return Result
	 */
	result_t upload_step(char *& buf, std::int32_t & buf_size)
	{
		return upload_step_impl(buf, buf_size, *obj_);
	}

private:
	std::unique_ptr<IObjView> obj_;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_UPLOADER_TO_BUFF_H
