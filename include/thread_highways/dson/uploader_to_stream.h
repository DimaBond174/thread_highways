/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_UPLOADER_TO_STREAM_H
#define THREADS_HIGHWAYS_DSON_UPLOADER_TO_STREAM_H

#include <thread_highways/dson/detail/obj_view.h>
#include <thread_highways/dson/detail/i_uploader.h>
#include <thread_highways/tools/result_code.h>

#include <algorithm> // std::copy
#include <fstream>
#include <iterator> // std::ostream_iterator

namespace hi
{
namespace dson
{

class UploaderToStream : public detail::DefaultUploader
{
public:
	result_t upload(std::ofstream & out, IObjView & obj)
	{
		reset_uploader();
		finish_offset_ = obj.all_size();
		auto res = okInProcess;
		while (okInProcess == res)
		{
			res = upload_step_impl(out, obj);
		}
		return res;
	}

protected:
	result_t upload_step_impl(std::ofstream & out, IObjView & obj)
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
			return upload_step_impl(out, obj);
		case State::Next:
			return eFail;
		case State::UploadingFromLocalBuf:
			return upload_impl(out, local_buf_, obj);
		case State::UploadingFromRemoteBuf:
			return upload_impl(out, remote_chunk_, obj);
		} // switch
		return eFail;
	} // upload_step_impl

private:
	result_t upload_impl(std::ofstream & out, const char * buf, IObjView & obj)
	{
		auto to_upload = remote_chunk_size_ - chunk_offset_;
		auto begin = buf + chunk_offset_;
		std::copy(begin, begin + to_upload, std::ostream_iterator<char>(out));
		chunk_offset_ += to_upload;
		// cur_offset_ += to_upload; сейчас выгружаю из чанка, а cur_offset уже был проставлен

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
		throw hi::Exception(
			"Error chunk_offset_=" + std::to_string(chunk_offset_)
				+ ", remote_chunk_size_=" + std::to_string(remote_chunk_size_),
			__FILE__,
			__LINE__);
		return eFail;
	}

	result_t set_next_chunk(IObjView & obj)
	{
		auto res = obj.upload_to(cur_offset_, *this);
		if (res < 0)
			state_ = State::Error;
		return res;
	}
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_UPLOADER_TO_STREAM_H
