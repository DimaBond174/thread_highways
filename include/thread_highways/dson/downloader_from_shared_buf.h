/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_SHARED_BUF_H
#define THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_SHARED_BUF_H

#include <thread_highways/dson/dson_from_buff.h>
#include <thread_highways/dson/dson_from_file.h>

#include <memory>

namespace hi
{
namespace dson
{

class DownloaderFromSharedBuf
{
public:
	// Настройка загрузки больших Dson-ов
	void setup_dson_from_file(std::shared_ptr<DsonFromFileController> controller)
	{
		max_size_ = controller->max_size();
		dson_from_file_controller_ = std::move(controller);
	}

	void clear()
	{
		state_ = State::Empty;
		offset_ = 0;
		dson_.reset();
	}

	std::unique_ptr<NewDson> get_result()
	{
		return std::move(dson_);
	}

	std::shared_ptr<NewDson> get_shared_result()
	{
		if (state_ != State::Ready)
			return {};
		auto re = std::make_shared<NewDson>(std::move(*dson_));
		clear();
		return re;
	}

	/*
	 * чтение из буфера (SharedMem или иного)
	 * @param buf буфер
	 * @param buf_size размер буфера
	 * @return Result
	 * @note загрузка происходит через окно буфера, окно множет быть много меньше чем передаваемый объём
	 * (например через shared mem)
	 * @note ожидается что в буфере уже есть Dson (текущий или заголовок нового)
	 * (ожидание появления Dson, сброс в случае ошибки - должны обеспечиваться внешними механизмами)
	 * @note за раз передаётся 1 Dson (нет механизмов загрузки с буфера сразу нескольких Dson,
	 *  - либо уже идёт загрузка и размер Dson берётся из его заголовка
	 *  - либо ожидается заголовок из которого будет понятен размер Dson)
	 * @note если Dson больше по размеру чем buf_size, то ожидается что buf заполнен полностью
	 */
	result_t load(char * buf, std::int32_t buf_size)
	{
		switch (state_)
		{
		case State::Error:
			clear();
			[[fallthrough]];
		case State::Empty:
			[[fallthrough]];
		case State::LoadingHeader:
			{
				auto res = load_header(buf, buf_size);
				if (state_ != State::LoadingData)
					return res;
				if (!buf_size)
				{
					return okInProcess;
				}
			}
			[[fallthrough]];
		case State::LoadingData:
			return load_body(buf, buf_size);
		default:
			break;
		} // switch
		state_ = State::Error;
		HI_ASSERT(false);
		return eFail;
	}

private:
	result_t load_header(char *& buf, std::int32_t & buf_size)
	{
		HI_ASSERT(buf_size > 0 && buf);
		// if (buf_size <= 0 || !buf) return eWrongParams;
		auto readed = detail::header_size - offset_;
		if (readed > buf_size)
			readed = buf_size;
		std::memcpy(header_ + offset_, buf, readed);
		offset_ += readed;
		if (offset_ < detail::header_size)
		{
			return okInProcess;
		}

		DsonHeader header;
		detail::get(header_, header);
		HI_ASSERT(detail::is_dson_header(header));
		//          if (detail::error_header(header))
		//          {
		//                state_ = State::Error;
		//                return eFailWrongHeader;
		//            }

		if (header.data_size_ == 0)
		{
			dson_ = std::make_unique<NewDson>();
			dson_->set_key(header.key_);
			state_ = State::Ready;
			return okReady;
		}

		if (header.data_size_ > max_size_) // проверять dson_from_file_controller_ не надо из-за default для max_size_
		{
			dson_ = dson_from_file_controller_->create_new();
		}
		else
		{
			dson_ = std::make_unique<DsonFromBuff>();
		}

		ICanUploadIntoDson * dson = dynamic_cast<ICanUploadIntoDson *>(dson_.get());
		// if (!dson) return eFailDynamicCast;
		auto res = dson->set_header(header);

		if (res < 0)
		{
			state_ = State::Error;
			return res;
		}

		state_ = State::LoadingData;
		buf_size -= readed;
		buf += readed;
		all_size_ = header.data_size_ + detail::header_size;
		return okInProcess;
	}

	result_t load_body(char * buf, std::int32_t buf_size)
	{
		auto size = all_size_ - offset_;
		if (buf_size > size)
			buf_size = size;
		ICanUploadIntoDson * dson = dynamic_cast<ICanUploadIntoDson *>(dson_.get());
		auto res = dson->upload_chunk(buf, buf_size);
		if (res == okReady)
		{
			offset_ = 0;
			state_ = State::LoadingHeader;
		}
		else if (res == okInProcess)
		{
			offset_ += buf_size;
		}
		else
		{
			state_ = State::Error;
		}
		return res;
	}

private:
	char header_[detail::header_size];
	std::unique_ptr<NewDson> dson_;
	std::shared_ptr<DsonFromFileController> dson_from_file_controller_;

	// Итератор загрузки
	std::int32_t offset_{0};
	std::int32_t all_size_{0};
	std::int32_t max_size_{2147483647};

	// Возможные состояния
	enum class State
	{
		// готов к использованию
		Empty,
		// идёт загрузка заголовка
		LoadingHeader,
		// идёт загрузка данных в Dson который в RAM
		LoadingData,
		// готов Dson который в RAM
		Ready,
		// сломался
		Error
	};
	State state_{State::Empty};
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_SHARED_BUF_H
