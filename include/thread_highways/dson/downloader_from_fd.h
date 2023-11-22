/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_FD_H
#define THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_FD_H

#include <thread_highways/dson/dson_from_buff.h>
#include <thread_highways/dson/dson_from_file.h>
#include <thread_highways/tools/fd_write_read.h>
#include <thread_highways/tools/result_code.h>

#include <chrono>

namespace hi
{
namespace dson
{

// POSIX чтение из fd (сеть/файл/pipe/..)
class DownloaderFromFD
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
	 * @brief load_from_fd
	 * POSIX чтение из fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 * @note загрузка происходит по мере возможности fd
	 * @note итераторы хранятся внутри - чтобы сбросить: clear()
	 */
	result_t load(const std::int32_t fd)
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
				auto res = load_header(fd);
				if (state_ != State::LoadingData)
					return res;
			}
			[[fallthrough]];
		case State::LoadingData:
			return load_body(fd);
		default:
			break;
		} // switch
		state_ = State::Error;
		HI_ASSERT(false);
		return eFail;
	}

private:
	result_t load_header(const std::int32_t fd)
	{
		const auto readed = read_from_fd(fd, header_ + offset_, detail::header_size - offset_);
		HI_ASSERT(readed >= 0);
		//            if (readed < 0)
		//            {
		//                state_ = State::Error;
		//                return eFailReadFromFD;
		//            }
		offset_ += readed;
		if (offset_ < detail::header_size)
		{
			return okInProcess;
		}

		DsonHeader header;
		detail::get(header_, header);
		HI_ASSERT(detail::is_dson_header(header));
		//            if (detail::error_header(header))
		//            {
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
		auto res = dson->set_header(header);

		if (res < 0)
		{
			state_ = State::Error;
			return res;
		}

		state_ = State::LoadingData;
		all_size_ = header.data_size_ + detail::header_size;
		return okInProcess;
	}

	result_t load_body(const std::int32_t fd)
	{
		ICanUploadIntoDson * dson = dynamic_cast<ICanUploadIntoDson *>(dson_.get());
		auto buf_size = dson->max_chunk_size();
		auto size = all_size_ - offset_;
		if (buf_size > size)
			buf_size = size;
		char * buf = dson->upload_with_uploader_buf(buf_size);
		if (!buf)
			return eFail;
		const auto readed = read_from_fd(fd, buf, buf_size);
		HI_ASSERT(readed >= 0);
		//        if (readed < 0)
		//        {
		//            state_ = State::Error;
		//            return eFailReadFromFD;
		//        }

		auto res = dson->set_uploaded(readed);
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

#endif // THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_FD_H
