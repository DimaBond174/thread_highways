/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DSON_FROM_BUFF_H
#define THREADS_HIGHWAYS_DSON_DSON_FROM_BUFF_H

#include <thread_highways/dson/new_dson.h>

#include <cstdlib> // malloc
#include <cstring> // memcpy

//#include <iostream>

namespace hi
{
namespace dson
{

// Dson поверх буфера, например полученного с сети
class DsonFromBuff
	: public NewDson
	, public ICanUploadIntoDson
{
public:
	DsonFromBuff()
	{
		clear();
	}

	DsonFromBuff(const DsonFromBuff &) = delete;
	DsonFromBuff(DsonFromBuff && other)
	{
		move_from_other(std::move(other));
	}

	DsonFromBuff & operator=(const DsonFromBuff &) = delete;
	DsonFromBuff & operator=(DsonFromBuff && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		move_from_other(std::move(other));
		return *this;
	}

	~DsonFromBuff()
	{
		clear();
	}

	result_t upload(IDson & dson)
	{
		clear();
		const auto size = dson.all_size();
		key_ = dson.obj_key();
		state_ = State::StartPosition;
		start_offset_ = cur_offset_ = 0;
		finish_offset_ = size;
		RETURN_CODE_IF_FAIL(allocate(size));

		while (cur_offset_ < finish_offset_)
		{
			RETURN_CODE_IF_FAIL(dson.upload_to(cur_offset_, *this));
			if (state_ == State::Error)
				return eFail;
		}
		HI_ASSERT(cur_offset_ == finish_offset_);
		// load_header(); нуже был только key, его поставили выше
		return ok;
	}

public: // ICanUploadIntoDson
	// Установка заголовка и аллокация ресурсов под приём данных
	result_t set_header(const DsonHeader & header) override
	{
		HI_ASSERT(detail::is_dson_header(header));
		key_ = header.key_;
		data_size_without_new_dson_ = header.data_size_;
		state_ = State::Next;
		start_offset_ = cur_offset_ = detail::header_size;
		finish_offset_ = header.data_size_ + detail::header_size;
		RETURN_CODE_IF_FAIL(allocate(finish_offset_));
		save_header(header);
		return ok;
	}

	// Максимальный объём данных исходя из возможностей и потребностей в header
	std::int32_t max_chunk_size() override
	{
		return finish_offset_ - cur_offset_;
	}

	// Пользователь предоставляет свой буффер откуда можно забрать данные
	result_t upload_chunk(const char * chunk, std::int32_t chunk_size) override
	{
		HI_ASSERT(cur_offset_ + chunk_size <= finish_offset_);
		//        if (cur_offset_ + chunk_size > finish_offset_) // || chunk_size <= 0 || !chunk)
		//        {
		////            std::cout << "cur_offset_ + chunk_size = " << (cur_offset_ + chunk_size)
		////                << ", finish_offset_ == " << finish_offset_ << std::endl;
		//            state_ = State::Error;
		//            return eWrongParams;
		//        }

		std::memcpy(buf_ + cur_offset_, chunk, chunk_size);
		cur_offset_ += chunk_size;
		if (cur_offset_ == finish_offset_)
		{
			start_offset_ = finish_offset_;
			state_ = State::Next;
			return okReady;
		}
		HI_ASSERT(cur_offset_ < finish_offset_);
		state_ = State::UploadingFromRemoteBuf;
		return okInProcess;
	}

	// Пользователь запрашивает буфер для выгрузки данных
	char * upload_with_uploader_buf(std::int32_t chunk_size) override
	{
		if (chunk_size <= 0 || cur_offset_ + chunk_size > finish_offset_)
		{
			state_ = State::Error;
			return nullptr;
		}
		state_ = State::UploadingFromLocalBuf;
		return buf_ + cur_offset_;
	}

	// Пользователь фиксирует какой объём был загружен в предоставленный буффер
	result_t set_uploaded(std::int32_t uploaded) override
	{
		HI_ASSERT(uploaded >= 0);
		//        if (uploaded < 0)
		//        {
		//            state_ = State::Error;
		//            return eWrongParams;
		//        }

		cur_offset_ += uploaded;
		if (cur_offset_ == finish_offset_)
		{
			state_ = State::Next;
			return okReady;
		}

		HI_ASSERT(cur_offset_ <= finish_offset_);
		//        if (cur_offset_ > finish_offset_)
		//        {
		//            state_ = State::Error;
		//            return eWrongParams;
		//        }
		return okInProcess;
	}

	void reset_uploader() override
	{
		// todo
	}

public: // IDson
	void clear() override
	{
		if (buf_)
		{
			free(buf_);
			buf_ = nullptr;
			buf_size_ = 0;
		}
		data_size_without_new_dson_ = 0;
		NewDson::clear();
	}

	bool find(const Key key, const std::function<void(IObjView *)> & fun) override
	{
		if (NewDson::find(key, fun))
			return true;
		return detail::TempDsonFromBuff::find(buf_, buf_size_, key, fun);
	}

	std::unique_ptr<IDson> move_self() override
	{
		return std::make_unique<DsonFromBuff>(std::move(*this));
	}

	//    result_t emplace_dson(std::unique_ptr<IObjView> obj) override
	//    {
	//        const auto key = obj->obj_key();
	//        const auto res = NewDson::emplace_dson(std::move(obj));
	//        if (res == okReplaced) return res; // Если такой уже был в NewDson, то уже точно не было в файле

	//        char* found = find(key);
	//        if (found)
	//        { // есть такой в буффере и сейчас map его перебъёт, поэтому зачищаю
	//            DsonHeader header;
	//            detail::get(found, header);
	////            data_size_without_new_dson_ -= header.data_size_;
	////            header.key_ = static_cast<std::int32_t>(SystemKey::Deleted);
	////            header.data_type_ = detail::types_map<detail::NoType>::value;
	////            detail::put(found, header);
	//            header_.data_size_ -= header.data_size_;
	//            HI_ASSERT(header_.data_size_ >= 0);
	//        }
	//        return res;
	//    }

	bool has(const Key key) override
	{
		return find(key, [](IObjView *) {});
	}

	// Обход объектов, в колбэк будут переданы хидера один за другим
	void private_iterate(const std::function<void(std::unique_ptr<IObjView>)> & callback) override
	{
		HI_ASSERT(buf_);
		NewDson::private_iterate(callback);
		detail::TempDsonFromBuff::for_all_in_buf(
			buf_,
			buf_size_,
			[&](const DsonHeader & header, std::int32_t cur_file_offset) -> bool
			{
				if (NewDson::has(header.key_))
					return false;
				if (header.data_type_ == detail::types_map<detail::DsonContainer>::value)
				{
					callback(std::make_unique<detail::TempDsonFromBuff>(buf_ + cur_file_offset));
				}
				else
				{
					callback(std::make_unique<DsonObjView>(buf_ + cur_file_offset));
				}
				return false; // итерируй дальше
			});
	}

public: // IObjView
	std::int32_t data_size() override
	{
		auto size = NewDson::data_size();
		detail::TempDsonFromBuff::for_all_in_buf(
			buf_,
			buf_size_,
			[&](const DsonHeader & header, std::int32_t /*cur_file_offset*/) -> bool
			{
				if (NewDson::has(header.key_))
					return false;
				size += header.data_size_ + detail::header_size;
				return false; // итерируй дальше
			});
		return size;
	}

	result_t upload_to(const std::int32_t at, detail::IUploader & uploader) override
	{
		auto res = NewDson::upload_to(at, uploader);
		if (res != eFailMoreThanIHave)
			return res;
		return upload_buf_to(at - map_it_to_ + detail::header_size, uploader);
	}

protected:
	void move_from_other(DsonFromBuff && other)
	{
		if (buf_)
			free(buf_);
		buf_ = other.buf_;
		buf_size_ = other.buf_size_;
		other.buf_ = nullptr;
		other.buf_size_ = 0;

		NewDson::move_from_other(std::move(other));
	}

private:
	result_t allocate(const std::int32_t size)
	{
		HI_ASSERT(size >= detail::header_size);
		// if (size < detail::header_size) return eWrongParams;
		buf_ = static_cast<char *>(malloc(size));
		if (!buf_)
			return eNoMemory;
		buf_size_ = size;
		return ok;
	}

	void save_header(const DsonHeader & header)
	{
		detail::put(buf_, header);
	}

	//    result_t load_header()
	//    {
	//        detail::get(buf_, header_);
	//        data_size_without_new_dson_ = header_.data_size_;
	//        return ok;
	//    }

	//    char* find(const Key key)
	//    {
	//    for_all_in_buf
	//        if (!buf_) return nullptr;
	//        char* cur_header = buf_;
	//        char* const end = buf_ + buf_size_;
	//        DsonHeader header;
	//        do {
	//            detail::get(cur_header, header);
	//            if (key == header.key_) return cur_header;
	//            cur_header = cur_header + header.data_size_ + detail::header_size;
	//        } while(cur_header < end);
	//        return nullptr;
	//    } // find

	result_t upload_buf_to(std::int32_t at, detail::IUploader & uploader)
	{
		HI_ASSERT(at >= detail::header_size);

		State state{State::StartPosition};
		std::int32_t cur_at{detail::header_size};
		std::int32_t start_window{0};
		std::int32_t end_window{0};
		const auto max_window = buf_size_;

		detail::TempDsonFromBuff::for_all_in_buf(
			buf_,
			buf_size_,
			[&](const DsonHeader & header, std::int32_t cur_file_offset) -> bool
			{
				switch (state)
				{
				case State::StartPosition:
					{ // Поиск начала выгрузки == первый объект которого нет в NewDson
						if (NewDson::has(header.key_))
							return false; // проматываю отправленные из NewDson
						const auto distance_to_next = header.data_size_ + detail::header_size;
						const auto next_at = cur_at + distance_to_next;
						if (next_at <= at)
						{
							cur_at = next_at;
							return false; // проматываю отправленные из файла
						}
						const auto diff = at - cur_at; // сколько от текущей позиции до начала окна
						HI_ASSERT(diff >= 0);
						start_window = cur_file_offset + diff;
						end_window = cur_file_offset + distance_to_next;
						HI_ASSERT(end_window > start_window);

						state = State::Next;
						return false; // надо возвращать false чтобы итерации продолжились
					}

				case State::Next:
					// Поиск конца выгрузки == первый объект которого есть в NewDson
					{
						if (NewDson::has(header.key_))
							return true; // закончили итерации
						const auto next_end_window = end_window + header.data_size_ + detail::header_size;
						const auto window_size = next_end_window - start_window;
						if (max_window < window_size)
						{
							end_window = start_window + max_window;
							return true; // закончили итерации
						}
						end_window = next_end_window;
						return false; // продолжаем двигать конец окна
					}
				default:
					HI_ASSERT(false);
					break;
				};
			});

		if (!end_window)
		{
			// нечего выгружать
			return okReady;
		}

		std::int32_t target_window_size = end_window - start_window;
		HI_ASSERT(end_window <= buf_size_);
		auto buf = buf_ + start_window;
		return uploader.upload_chunk(buf, target_window_size);
	}

private:
	// Буффер вместе с header (header.data_size_ + detail::header_size)
	char * buf_{nullptr};

	// Размер buf_
	std::int32_t buf_size_{0};
	std::int32_t data_size_without_new_dson_{0};
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DSON_FROM_BUFF_H
