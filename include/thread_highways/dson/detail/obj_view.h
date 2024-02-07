/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_VIEW_H
#define THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_VIEW_H

#include <thread_highways/dson/detail/header.h>
#include <thread_highways/dson/detail/i_uploader.h>
#include <thread_highways/tools/exception.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring> // memcpy

namespace hi
{
namespace dson
{

// IObjView -
// Ака std::string_view == просмотр без копирования
// IObjView : размер и итератор по байтам
/*
	IObjView - просмотр объекта хранящегося в NewDson|FileDson|NetworkDson
	Используется для получения доступа к байтам и в кастинге к Host объектам (std::string_view|number|..)
	Не меняет исходный буффер.
	HostOrder хранить нет смысла: при обращении через get<T>(T& obj) буфер в который надо переложить уже подаётся.

	Итератор нужен только для выгрузки. Выгрузка может быть только в host|network oder == файл можно открывать где
   угодно. Большие объёмы идут в uint8, нет преобразования в network..
*/
struct IObjView
{
	/*
	 * Итератор по байтам объекта. Удобно для выгрузки в стандартный STL поток,
	 * но лучше выгружать кусками буфера через char* at(const std::int32_t i, std::int32_t& buf_size)
	 */
	//    class iterator: public std::iterator<std::input_iterator_tag, char>
	//    {
	//        std::reference_wrapper<IObjView> ref_;
	//        std::int32_t i_{0};
	//    public:
	//        explicit iterator(std::reference_wrapper<IObjView> ref, std::int32_t i)
	//            : ref_{std::move(ref)}
	//            , i_{i}
	//        {}

	//        iterator& operator++() {++i_; return *this;}
	//        iterator operator++(int) {iterator retval = *this; ++(*this); return retval;}
	//        bool operator==(iterator other) const
	//        {
	//            return i_ == other.i_;
	//        }
	//        bool operator!=(iterator other) const {return !(*this == other);}
	//        char operator*() const
	//        {
	//            std::int32_t size{0};
	//            char* buf = ref_.get().at(i_, size);
	//            // todo заменить на Hiway Exception
	//            //if (!buf) throw std::runtime_error("Overflow");
	//            assert(buf);
	//            return *buf;
	//        }
	//    };

	//    iterator begin() {return iterator(*this, 0);}
	//    iterator end() {return iterator(*this, all_size());}

	virtual ~IObjView() = default;

	// Тип хранимого объекта
	virtual std::int32_t data_type() = 0;

	// Размер хранимого объекта
	virtual std::int32_t data_size() = 0;

	// Ключ хранимого объекта
	virtual Key obj_key() = 0;

	template <typename K>
	K obj_key_cast()
	{
		return static_cast<K>(obj_key());
	}

	virtual std::int32_t all_size()
	{
		return detail::header_size + data_size();
	}

	// Содержимое любого объекта как последовательность байт
    virtual bool buf_view(BufUCharView& /*result*/, bool& /*with_header*/)
	{
		HI_ASSERT(false);
		return false;
	}

    bool buf_view_skip_header(BufUCharView& result)
    {
        bool with_header{false};
        bool re = buf_view(result, with_header);
        if (!re || !with_header) return re;

        const char *data = reinterpret_cast<const char *>(result.data());
        DsonHeader header;
        detail::get_and_shift(data, header);
        detail::get(data, header.data_size_, result);
        return re;
    }

	/**
	 * @brief upload_to
	 * @param at - с какой внутренней позиции объекта данные нужны
	 * @param uploader - куда выгружать
	 * @return код ошибки или статус выгрузки
	 *  = okInProcess - выгрузка ещё не окончена(например объект может предоставлять данные чанками)
	 *  = okReady - объект сообщил что полностью выгрузил своё содержимое
	 */
	virtual result_t upload_to(const std::int32_t /*at*/, detail::IUploader & /*uploader*/)
	{
		HI_ASSERT(false);
		return eFail;
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
	virtual IObjView * self()
	{
		return this;
	}

	/*
	 * Указатель для внутреннего употребления в MailBox, Dson и др. контейнерах.
		Наружу из контейнера всегда должны выходить только умные указатели или объекты
		- поэтому мувать содержимое для перехода во внутренний raw pointer и возврата наружу .
		Данный указатель необходим для работы с безмьютексными контейнерами.
	*/
	IObjView * next_in_stack_{nullptr};
};

namespace detail
{

struct RawIObjViewPtr : public IObjView
{
	RawIObjViewPtr(IObjView * ptr)
		: ptr_{ptr}
	{
	}
	IObjView * self() override
	{
		return ptr_;
	}

	std::int32_t data_type() override
	{
		return ptr_->data_type();
	}

	// Размер хранимого объекта
	std::int32_t data_size() override
	{
		return ptr_->data_size();
	}

	// Ключ хранимого объекта
	virtual Key obj_key() override
	{
		return ptr_->obj_key();
	}

	IObjView * ptr_;
};

} // namespace detail
} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_VIEW_H
