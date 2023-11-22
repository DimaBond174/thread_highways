/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DETAIL_I_UPLOADER_H
#define THREADS_HIGHWAYS_DSON_DETAIL_I_UPLOADER_H

#include <thread_highways/tools/result_code.h>
#include <thread_highways/tools/exception.h>

#include <cstdint>
#include <stdlib.h>

//#include <iostream>
//#include <thread_highways/dson/detail/header.h>

namespace hi
{
namespace dson
{
namespace detail
{

class IUploader
{
public:
	virtual ~IUploader() = default;

	// Пользователь предоставляет свой буффер откуда можно забрать данные
	virtual result_t upload_chunk(const char * chunk, std::int32_t chunk_size) = 0;

	// Пользователь запрашивает буфер для выгрузки данных
	virtual char * upload_with_uploader_buf(std::int32_t chunk_size) = 0;
	// Пользователь фиксирует какой объём был загружен в предоставленный буффер
	virtual result_t set_uploaded(std::int32_t uploaded) = 0;

	// Подготовка к новой загрузке
	virtual void reset_uploader() = 0;

protected:
	enum class State
	{
		// на старте
		StartPosition,
		// готов к следующему шагу
		Next,
		// идёт выгрузка из локального буфера
		UploadingFromLocalBuf,
		// идёт выгрузка из внешего буфера
		UploadingFromRemoteBuf,
		// загрузил всё, итераторы на финише
		Finished,
		// сломался
		Error
	};

	// Итераторы выгрузки - используются для одного текущего объекта в который подали IUploader
	// Смещения - адреса в целевом хранилище
	// Текущее смещение внутри выгружаемого объекта вычисляется как (cur_offset_ - start_offset_)
	std::int32_t start_offset_{0}; // Куда должен быть выгружен объект
	std::int32_t cur_offset_{0}; // Текущее смещение в целевом хранилище
	std::int32_t finish_offset_{0}; // Финишное смещение в целевом хранилище
	State state_{State::StartPosition};
};

class DefaultUploader : public IUploader
{
public:
	virtual ~DefaultUploader()
	{
		free_allocated();
	}

public: // IUploader
	// Пользователь предоставляет свой буффер откуда можно забрать данные
	result_t upload_chunk(const char * chunk, std::int32_t chunk_size) override
	{
		// Пользователь фиксирует какой объём был загружен в предоставленный буффер
		// ( в данном случае это уже готовый буфер от пользователя, но в upload_with_uploader_buf это не так)
		cur_offset_ += chunk_size;
		remote_chunk_ = chunk;
		remote_chunk_size_ = chunk_size;
		state_ = State::UploadingFromRemoteBuf;
		// Из локального буфера ещё надо выгрузить в целевое хранилище, а потом State::Finished & okReady
		return okInProcess;
	}

	// Пользователь запрашивает буфер для выгрузки данных
	char * upload_with_uploader_buf(std::int32_t chunk_size) override
	{
		if (chunk_size <= 0)
		{
			state_ = State::Error;
			return nullptr;
		}
		if (local_buf_size_ < chunk_size)
		{
			free_allocated();
			local_buf_ = static_cast<char *>(std::malloc(static_cast<std::size_t>(chunk_size)));
			if (!local_buf_)
			{
				state_ = State::Error;
				return nullptr;
			}

			local_buf_size_ = chunk_size;
		}
		state_ = State::UploadingFromLocalBuf;
		remote_chunk_size_ = chunk_size;
		return local_buf_;
	}

	// Пользователь фиксирует какой объём был загружен в предоставленный буффер
	result_t set_uploaded(std::int32_t uploaded) override
	{
		// if (uploaded == detail::header_size && (cur_offset_ - start_offset_) == 0)
		//{ // to remove
		//     detail::Header header;
		//     detail::get(local_buf_, header);
		//     std::cout << "set_uploaded cur_offset_=" << cur_offset_
		//     << ", header(key_=" << header.key_
		//     << ", data_size_=" << header.data_size_
		//     << ", data_type_=" << header.data_type_
		//     << std::endl;
		// }

		cur_offset_ += uploaded;
		HI_ASSERT(uploaded >= 0 && cur_offset_ <= finish_offset_);
		//        if (uploaded < 0 || cur_offset_ > finish_offset_)
		//        {
		//            state_ = State::Error;
		//            return eWrongParams;
		//        }

		// Из локального буфера ещё надо выгрузить в целевое хранилище, а потом   State::Finished
		//        if (cur_offset_ == finish_offset_)
		//        {
		//            state_ = State::Finished;
		//            return okReady;
		//        }
		return okInProcess;
	}

	// Подготовка к новой загрузке
	void reset_uploader() override
	{
		state_ = State::StartPosition;
		start_offset_ = 0;
		cur_offset_ = 0;
		finish_offset_ = 0;
		remote_chunk_ = nullptr;
		remote_chunk_size_ = 0;
		chunk_offset_ = 0;
	}

protected:
	void free_allocated()
	{
		if (local_buf_)
		{
			std::free(local_buf_);
			local_buf_ = nullptr;
		}
	}

protected:
	char * local_buf_{nullptr};
	const char * remote_chunk_{nullptr};
	std::int32_t local_buf_size_{0};
	std::int32_t remote_chunk_size_{0};
	// Смещение в буффере == прогресс выгрузки отсюда из local_buf_|remote_chunk_
	std::int32_t chunk_offset_{0};
};

} // namespace detail
} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DETAIL_I_UPLOADER_H
