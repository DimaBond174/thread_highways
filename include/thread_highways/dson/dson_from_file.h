/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_H
#define THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_H

#include <thread_highways/dson/new_dson.h>
#include <thread_highways/tools/buffer_truncator.h>
#include <thread_highways/tools/mem_map_proxy.h>
#include <thread_highways/tools/small_tools.h>

#include <cstdio> // rename
#include <cstring> // memcpy
#include <memory>
#include <string>

namespace hi
{
namespace dson
{

// Объект хранящийся в файле по определённому смещению
class ObjectInFile : public IObjView
{
public:
	ObjectInFile(std::shared_ptr<MemMapProxy> memfile, DsonHeader header, std::int32_t offset)
		: memfile_{std::move(memfile)}
		, header_{header}
		, offset_{offset}
	{
	}

	// Тип хранимого объекта
	std::int32_t data_type() override
	{
		return header_.data_type_;
	}

	std::int32_t data_size() override
	{
		return header_.data_size_;
	}

	Key obj_key() override
	{
		return header_.key_;
	}

	// Содержимое любого объекта как последовательность байт
	bool buf_view(BufUCharView & result) override
	{
		const auto size = header_.data_size_ + detail::header_size;
		char * full_obj = memfile_->get_window(offset_, size);
		HI_ASSERT(full_obj);
		// if (!full_obj) return eFailMemmap;
		result.init(full_obj, size);
		return true;
	}

	/**
	 * @brief upload_to
	 * @param at - с какой внутренней позиции объекта данные нужны
	 * @param uploader - куда выгружать
	 * @return код ошибки или статус выгрузки
	 *  = okInProcess - выгрузка ещё не окончена(например объект может предоставлять данные чанками)
	 *  = okReady - объект сообщил что полностью выгрузил своё содержимое
	 */
	virtual result_t upload_to(const std::int32_t at, detail::IUploader & uploader) override
	{
		const auto size = header_.data_size_ + detail::header_size - at;
		HI_ASSERT(size > 0);
		char * full_obj = memfile_->get_window(offset_ + at, size);
		HI_ASSERT(full_obj);
		// if (!full_obj) return eFailMemmap;
		return uploader.upload_chunk(full_obj, size);
	}

	/**
	 * @brief self
	 * Объекты хранятся в unique_ptr, в некоторых местах это временные окна на буфер или кусок файла.
	 * Иногда требуется этот временный указатель сохранить где-то ещё.
	 * shared_ptr вселял бы ложную веру в то что контроллирует содержимое + накладные расходы.
	 * raw указатель не позволяет использовать proxy объект который под капотом имеет доп. логику
	 * Поэтому всегда используются unique_ptr, но при этом иногда необходимо получить указатель на исходный объект,
	 * например чтобы закастить к IDson.
	 *
	 * @return указатель под капотом
	 */
	IObjView * self() override
	{
		return this;
	}

private:
	std::shared_ptr<MemMapProxy> memfile_;
	DsonHeader header_;
	std::int32_t offset_;
};

/*
	Предполагается что такой Dson собирается внешним объектом
	который скачивает или готовит к закачке Dson большого объёма.
	Предполагается что передаваться будет кусками транспортными Dson-ами
	меньшего размера в которых будут обратные координаты для восстановления докачки.
	Инициацию докачки будет ПО не транспортных роутеров, а уже ближе к клиенту/серверу
	где есть информация об актуальности и визуализация докачек.

	Нужен, к примеру, чтобы хранить информацию об инцидентах == видео + аналитика.
	Идея: делать ленивые Dson-ы внутри которых есть Dson-ы на закачку == в инциденте аналитика
	и фото разбора доступно прямо сразу, а тяжёлые части (видео) хранятся в виде ссылки на координаты где это можно
   скачать.

	Отложенный загрузчик больших файлов.
	Алгоритм работы:
	Куски большого Dson файла приезжают в составе Dson с обратными координатами докачки.
	(в координатах может быть что угодно - набор ID|полный путь к файлу на сервере и т.п.)
	У любого большого файла есть GUID - int64 под которым он будет сохранён в download_folder
*/
class DsonFromFile
	: public NewDson
	, public ICanUploadIntoDson
{
public:
	DsonFromFile() = default;
	DsonFromFile(std::string path)
	{
		HI_ASSERT(create(std::move(path)) > 0);
	}

	DsonFromFile(std::shared_ptr<MemMapProxy> memfile, std::int32_t dson_offset)
		: memfile_{std::move(memfile)}
		, dson_offset_{dson_offset}
	{
		load_header();
	}

	DsonFromFile(const DsonFromFile &) = delete;
	DsonFromFile(DsonFromFile && other)
	{
		move_from_other(std::move(other));
	}

	DsonFromFile & operator=(const DsonFromFile &) = delete;
	DsonFromFile & operator=(DsonFromFile && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		move_from_other(std::move(other));
		return *this;
	}

	~DsonFromFile()
	{
		clear();
	}

	std::int32_t current_file_size_before_save()
	{
		if (!memfile_)
			return 0;
		return memfile_->file_size();
	}

	result_t create(std::string path)
	{
		HI_ASSERT(dson_offset_ == 0); // такое нельзя делать во вложенном Dson
		clear();
		memfile_ = std::make_shared<MemMapProxy>();
		return memfile_->create(std::move(path));
	}

	result_t upload(IDson & dson)
	{
		// Зачистка текущего
		clear();
		// data_size_in_file_ = 0;
		// NewDson::clear();

		// Загрузка нового
		const auto size = dson.all_size();
		RETURN_CODE_IF_FAIL(memfile_->resize(size));

		start_offset_ = cur_offset_ = 0;
		state_ = State::StartPosition;
		finish_offset_ = size;
		while (cur_offset_ < finish_offset_)
		{
			RETURN_CODE_IF_FAIL(dson.upload_to(cur_offset_, *this));
			if (state_ == State::Error)
				return eFail;
		}
		HI_ASSERT(cur_offset_ == finish_offset_);
		load_header();
		return ok;
	}

	result_t reopen()
	{
		HI_ASSERT(dson_offset_ == 0); // return eCantDoThisInChildDson;
		auto res = memfile_->reopen();
		if (res != ok)
			return res;
		load_header();
		return ok;
	}

	virtual void open(std::string path)
	{
		clear();
		HI_ASSERT(dson_offset_ == 0); // return eCantDoThisInChildDson;
		memfile_ = std::make_shared<MemMapProxy>();
		auto res = memfile_->open(std::move(path));
		HI_ASSERT(okCreatedNew == res || ok == res);
		if (okCreatedNew == res)
			return;
		load_header();
	}

	// Временное размапирование от файла чтобы не держать ресурсы
	// пока Dson маршрутизируется
	void close()
	{
		if (memfile_)
		{
			memfile_->close();
		}
	}

	// Сохранение содержимого на диск (включая новые изменения)
	// Пока не вызван save файл неизменен
	//  == так можно большой файл (например аналитику с фото/видео) слать многим адресатам
	// добавляя новые необходимые поля и не сохраняя изменения.
	virtual void save_as(std::string path)
	{
		// 1. Сохраняю всё в новый файл
		std::string tmp_path = path + ".~tmp." + std::to_string(steady_clock_now().count());
		Finally finally{[&]
						{
							std::remove(tmp_path.c_str());
						}};

		{
			DsonFromFile tmp_dson{tmp_path};
			HI_ASSERT(tmp_dson.upload(*this) > 0); // todo void
		}

		// 2. Меняю файлы
		memfile_->close();
		std::remove(path.c_str());
		if (0 != std::rename(tmp_path.c_str(), path.c_str()))
		{
			throw Exception("std::rename fail from tmp_path=" + tmp_path + " to path=" + path, __FILE__, __LINE__);
		}

		// 3. Переоткрываю
		open(path);
	}

	virtual void save()
	{
		save_as(memfile_->path());
	}

public: // ICanUploadIntoDson
	// Установка заголовка и аллокация ресурсов под приём данных
	result_t set_header(const DsonHeader & header) override
	{
		HI_ASSERT(detail::is_dson_header(header));
		key_ = header.key_;
		state_ = State::Next;
		start_offset_ = cur_offset_ = detail::header_size;
		finish_offset_ = header.data_size_ + detail::header_size;
		RETURN_CODE_IF_FAIL(memfile_->resize(finish_offset_));
		save_header(header);
		return ok;
	}

	// Максимальный объём данных исходя из возможностей и потребностей в header
	std::int32_t max_chunk_size() override
	{
		const auto max_window = memfile_->get_max_window_size_for_file();
		HI_ASSERT(max_window > cur_offset_);
		return max_window - cur_offset_;
	}

	// Пользователь предоставляет свой буффер откуда можно забрать данные
	result_t upload_chunk(const char * chunk, std::int32_t chunk_size) override
	{
		HI_ASSERT(cur_offset_ + chunk_size <= finish_offset_);
		//        if (cur_offset_ + chunk_size > finish_offset_) // || chunk_size <= 0 || !chunk)
		//        {
		//            state_ = State::Error;
		//            return eWrongParams;
		//        }

		auto res = memfile_->copy_in(cur_offset_, chunk, chunk_size);
		if (res < 0)
		{
			state_ = State::Error;
			return res;
		}
		cur_offset_ += chunk_size;
		if (cur_offset_ == finish_offset_)
		{
			start_offset_ = finish_offset_;
			state_ = State::Next;
			return okReady;
		}
		state_ = State::UploadingFromRemoteBuf;
		return okInProcess;
	}

	// Пользователь запрашивает буфер для выгрузки данных
	char * upload_with_uploader_buf(std::int32_t chunk_size) override
	{
		HI_ASSERT(chunk_size > 0);

		char * buf = memfile_->get_window(cur_offset_, chunk_size);
		if (!buf)
		{
			state_ = State::Error;
			return nullptr;
		}
		state_ = State::UploadingFromLocalBuf;
		return buf;
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
		// memfile_->close();
		data_size_without_new_dson_ = 0;
		NewDson::clear();
	}

	bool find(const Key key, const std::function<void(IObjView *)> & fun) override
	{
		if (NewDson::find(key, fun))
			return true;
		bool found = false;
		for_all_in_file(
			[&](const DsonHeader & header, std::int32_t cur_file_offset) -> bool
			{
				if (header.key_ != key)
					return false;
				// В NewDson проверять не надо т.к. выше уже не нашли
				if (header.data_type_ == detail::types_map<detail::DsonContainer>::value)
				{
					auto obj = std::make_unique<DsonFromFile>(memfile_, cur_file_offset);
					fun(obj.get());
				}
				else
				{
					auto obj = std::make_unique<ObjectInFile>(memfile_, header, cur_file_offset);
					fun(obj.get());
				}

				found = true;
				return true; // дальше итерировать не надо
			});
		return found;
	}

	std::unique_ptr<IDson> move_self() override
	{
		return std::make_unique<DsonFromFile>(std::move(*this));
	}

	//    result_t emplace_dson(std::unique_ptr<IObjView> obj) override
	//    {
	//        const auto key = obj->obj_key();
	//        const auto res = NewDson::emplace_dson(std::move(obj));
	//        if (res == okReplaced) return res; // Если такой уже был в NewDson, то уже точно не было в файле
	//        char* found = find(key);
	//        if (found)
	//        { // когда буду выгружать потребуется сказать какой размер всего Dson => он должен быть в хид ере
	//            DsonHeader header;
	//            detail::get(found, header);
	//            // так нельзя data_size_in_file_ -= header.data_size_;
	////            header.key_ = static_cast<std::int32_t>(SystemKey::Deleted);
	////            header.data_type_ = detail::types_map<detail::NoType>::value;
	////            detail::put(found, header);

	//            // Отражаю что удалил этот объём
	//            header_.data_size_ -= header.data_size_;
	//            HI_ASSERT(header_.data_size_ >= 0);
	//        }
	//        return res;
	//    }

public: // IObjView
	std::int32_t data_size() override
	{
		auto size = NewDson::data_size();
		for_all_in_file(
			[&](const DsonHeader & header, std::int32_t /*cur_file_offset*/) -> bool
			{
				if (NewDson::has(header.key_))
					return false;
				size += header.data_size_ + detail::header_size;
				return false; // итерируй дальше
			});
		return size;
	}

	result_t upload_to(std::int32_t at, detail::IUploader & uploader) override
	{
		auto res = NewDson::upload_to(at, uploader);
		if (res != eFailMoreThanIHave)
			return res;

		// Обход файла с пропуском удалённых/переименованных
		return upload_file_to(at - map_it_to_ + detail::header_size, uploader);
		// return upload_file_to(at - map_it_to_, uploader);
	}

protected:
	void move_from_other(DsonFromFile && other)
	{
		memfile_ = std::move(other.memfile_);
		NewDson::move_from_other(std::move(other));
	}

	void load_header()
	{
		char * buf = memfile_->get_window(dson_offset_, detail::header_size);
		HI_ASSERT(buf);
		// if (!header) return eFailMemmap;
		DsonHeader header;
		detail::get(buf, header); // NewDson header
		HI_ASSERT(detail::is_dson_header(header));

		data_size_without_new_dson_ = header.data_size_;
		key_ = header.key_;
	}

	void private_iterate(const std::function<void(std::unique_ptr<IObjView>)> & callback) override
	{
		NewDson::private_iterate(callback);
		for_all_in_file(
			[&](const DsonHeader & header, std::int32_t cur_file_offset) -> bool
			{
				if (NewDson::has(header.key_))
					return false;
				if (header.data_type_ == detail::types_map<detail::DsonContainer>::value)
				{
					callback(std::make_unique<DsonFromFile>(memfile_, cur_file_offset));
				}
				else
				{
					callback(std::make_unique<ObjectInFile>(memfile_, header, cur_file_offset));
				}
				return false; // итерируй дальше
			});
	}

private:
	void for_all_in_file(const std::function<bool(const DsonHeader & header, std::int32_t cur_file_offset)> & fun)
	{
		if (!memfile_)
			return;
		auto file_size = memfile_->file_size();
		if (0 == file_size)
			return;
		HI_ASSERT(file_size >= dson_offset_ + detail::header_size);
		std::int32_t cur_offset{dson_offset_}; // заголовок Dson нужен чтобы узнать размер вложенного Dson
		char * cur_header = memfile_->get_window(cur_offset, detail::header_size);
		HI_ASSERT(cur_header);
		DsonHeader header;
		detail::get(cur_header, header);
		cur_offset += detail::header_size;
		const auto max_offset = header.data_size_ + cur_offset;
		HI_ASSERT(header.data_size_ >= 0 && file_size >= max_offset);
		while (cur_offset < max_offset)
		{
			cur_header = memfile_->get_window(cur_offset, detail::header_size);
			HI_ASSERT(cur_header);
			detail::get(cur_header, header);
			HI_ASSERT(detail::is_ok_header(header));
			// Если функция нашла что хотела, то выходим
			if (fun(header, cur_offset))
				return;
			// if (key == header.key_) return cur_header;
			// if (header.data_size_ < 0) return nullptr;
			cur_offset += header.data_size_ + detail::header_size;
		}
		HI_ASSERT(cur_offset == max_offset);
	}

	//    char* find(const Key key)
	//    {
	//        auto file_size = memfile_->file_size();
	//        if (0 == file_size) return nullptr;
	//        // выше по стеку только что добавил новое поле HI_ASSERT(file_size == header_.data_size_ +
	//        detail::header_size);
	// //       if (file_size != header_.data_size_ + detail::header_size) return nullptr;
	//        std::int32_t cur_offset{detail::header_size + dson_offset_}; // проматываю заголовок Dson
	//        DsonHeader header;
	//        while (cur_offset < file_size)
	//        {
	//            char* cur_header = memfile_->get_window(cur_offset, detail::header_size);
	//            detail::get(cur_header, header);
	//            if (key == header.key_) return cur_header;
	//            HI_ASSERT(header.data_size_ >= 0);
	//            //if (header.data_size_ < 0) return nullptr;
	//            cur_offset += header.data_size_ + detail::header_size;
	//        }
	//        return nullptr;
	//    } // find

	void save_header(const DsonHeader & header)
	{
		char * cur_header = memfile_->get_window(0, detail::header_size);
		HI_ASSERT(cur_header);
		// if (!cur_header) return eFailMemmap;
		detail::put(cur_header, header);
	}

	std::int32_t get_real_size_dson_in_file()
	{
		if (!memfile_)
		{ // Пустой файл основного Dson
			return 0;
		}
		auto file_size = memfile_->file_size();
		if (file_size == 0 && dson_offset_ == 0)
		{ // Пустой файл основного Dson
			return 0;
		}
		HI_ASSERT(dson_offset_ + detail::header_size <= file_size);
		//        if (dson_offset_ + detail::header_size > file_size)
		//        {
		//            return eCorruptedFile;
		//        }
		char * header_ptr = memfile_->get_window(dson_offset_, detail::header_size);
		HI_ASSERT(header_ptr);
		return detail::get_data_size(header_ptr) + detail::header_size;
	}

	result_t upload_file_to(std::int32_t at, detail::IUploader & uploader)
	{
		HI_ASSERT(at >= detail::header_size);

		State state{State::StartPosition};
		std::int32_t cur_at{detail::header_size};
		std::int32_t start_window{0};
		std::int32_t end_window{0};
		const auto max_window = memfile_->get_max_window_size_for_file();

		for_all_in_file(
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
		std::int32_t result_window_size{0};
		auto buf = memfile_->get_window(start_window, target_window_size, result_window_size);
		HI_ASSERT(target_window_size == result_window_size);
		HI_ASSERT(buf);
		return uploader.upload_chunk(buf, result_window_size);
	}

protected:
	std::shared_ptr<MemMapProxy> memfile_;

	// В случае если это вложенный в основной Dson, тут будет смещение в файле основного Dson
	std::int32_t dson_offset_{0};

	// Размер данных в файле (без учёта новых изменений/удалений)
	// std::int32_t data_size_in_file_{0};
	std::int32_t data_size_without_new_dson_{0};
};

// Управление границей размера Dson когда необходимо работать уже через файл
// Определение пути для DsonFromFile
class DsonFromFileController
{
public:
	DsonFromFileController(std::int32_t max_size, std::string folder)
		: cur_file_id_{static_cast<std::uint64_t>(steady_clock_now().count())}
		, folder_{std::move(folder)}
		, max_size_{max_size}
	{
	}

	virtual ~DsonFromFileController() = default;

	std::int32_t max_size()
	{
		return max_size_;
	}

	virtual std::unique_ptr<DsonFromFile> create_new()
	{
		std::unique_ptr<DsonFromFile> re = std::make_unique<DsonFromFile>();
		re->open(folder_ + std::to_string(++cur_file_id_));
		return re;
	}

protected:
	std::uint64_t cur_file_id_;
	std::string folder_;

	// Размер после которого переключаться на выгрузку в файл
	std::int32_t max_size_;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_H
