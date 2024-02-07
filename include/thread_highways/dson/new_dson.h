/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_NEW_DSON_H
#define THREADS_HIGHWAYS_DSON_NEW_DSON_H

#include <thread_highways/dson/i_dson.h>

#include <cstring> // memcpy
#include <map>
#include <memory>
#include <optional>

//#include <iostream>

namespace hi
{
namespace dson
{
namespace detail
{

class TempDsonFromBuff : public IDson
{
public:
	TempDsonFromBuff(char * buf)
		: buf_{buf}
	{
		assert(buf_);
	}

	char * buf()
	{
		return buf_;
	}

	static std::unique_ptr<IObjView> to_view(char * buf)
	{
		const auto type = detail::get_data_type(buf);
		switch (type)
		{
		case detail::types_map<detail::DsonContainer>::value:
			return std::make_unique<detail::TempDsonFromBuff>(buf);
		default:
			break;
		}

		return std::make_unique<DsonObjView>(buf);
	}

public: // IDson
	void clear() override
	{
	}

	void set_key(const Key key) override
	{
		detail::put_key(buf_, key);
	}

	static void for_all_in_buf(
		char * const buf,
		const std::int32_t buf_size,
		const std::function<bool(const DsonHeader & header, std::int32_t cur_file_offset)> & fun)
	{
		HI_ASSERT(buf && buf_size >= detail::header_size);
		DsonHeader header;
		detail::get(buf, header);
		HI_ASSERT(header.data_size_ < buf_size);

		char * cur_header = buf + detail::header_size;
		char * const end = cur_header + header.data_size_;

		while (cur_header < end)
		{
			detail::get(cur_header, header);
			HI_ASSERT(detail::is_ok_header(header));
			// Если функция нашла что хотела, то выходим
			if (fun(header, cur_header - buf))
				return;

			cur_header += header.data_size_ + detail::header_size;
		}
		HI_ASSERT(cur_header == end);
	}

	static bool find(
		char * buf,
		const std::int32_t buf_size,
		const Key key,
		const std::function<void(IObjView *)> & fun)
	{
		bool found = false;
		for_all_in_buf(
			buf,
			buf_size,
			[&](const DsonHeader & header, std::int32_t cur_file_offset) -> bool
			{
				if (header.key_ != key)
					return false;
				// В NewDson проверять не надо т.к. выше уже не нашли
				if (header.data_type_ == detail::types_map<detail::DsonContainer>::value)
				{
					auto obj = std::make_unique<detail::TempDsonFromBuff>(buf + cur_file_offset);
					fun(obj.get());
				}
				else
				{
					auto obj = std::make_unique<DsonObjView>(buf + cur_file_offset);
					fun(obj.get());
				}

				found = true;
				return true; // дальше итерировать не надо
			});
		return found;
	} // find

	bool find(const Key key, const std::function<void(IObjView *)> & fun) override
	{
		return TempDsonFromBuff::find(buf_, detail::get_data_size(buf_) + detail::header_size, key, fun);
	}

	bool has(const Key key) override
	{
		return find(key, [](IObjView *) {});
	}

	std::unique_ptr<IDson> move_self() override
	{
		return std::make_unique<TempDsonFromBuff>(buf_);
	}

	void emplace_dson(std::unique_ptr<IObjView> /*obj*/) override
	{
		HI_ASSERT(false);
	}

	void private_iterate(const std::function<void(std::unique_ptr<IObjView>)> &) override
	{
		HI_ASSERT(false); // должен использовать реальные DsonFile и т.п.
	}

public: // IObjView
	std::int32_t data_size() override
	{
		return detail::get_data_size(buf_);
	}

	Key obj_key() override
	{
		return detail::get_key(buf_);
	}

	IObjView * self() override
	{
		return this;
	}

private:
	// Временный указатель на внешний буффер
	char * buf_{nullptr};
};

} // namespace detail

// другие должны от него наследоваться чтобы переиспользовать стэк
// todo: может его в IDson? - решат вопросы выгрузки/загрузки
class NewDson : public IDson
{
public:
	NewDson()
	{
		clear();
	}

	NewDson(const NewDson &) = delete;
	NewDson(NewDson && other)
	{
		move_from_other(std::move(other));
	}

	NewDson & operator=(const NewDson &) = delete;
	NewDson & operator=(NewDson && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		move_from_other(std::move(other));
		return *this;
	}

	//    DsonHeader& header()
	//    {
	//        return header_;
	//    }

	// Для DsonEditController при открытии Dson чтобы убедиться что
	// не заменяем реальный Dson его представлением - если он уже есть в Dson, значит менять не надо
	void try_emplace(std::unique_ptr<IObjView> obj)
	{
		auto key = obj->obj_key();
		if (map_.find(key) != map_.end())
			return;
		// header_.data_size_ -= obj->all_size();
		// HI_ASSERT(header_.data_size_ >= 0);
		map_.emplace(key, std::move(obj));
        map_it_.reset();
	}

    virtual void rename_item(const Key old_key, const Key new_key)
    {
        if (old_key == new_key) return;
        auto it = map_.find(old_key);
        HI_ASSERT(it != map_.end());
        // Пока возможность изменить ключ есть только у IDson
        HI_ASSERT(it->second->self()->data_type() == detail::types_map<detail::DsonContainer>::value);
        auto dson = dynamic_cast<IDson *>(it->second->self());
        HI_ASSERT(dson);
        dson->set_key(new_key);
        map_[new_key] = std::move(it->second);
        map_[old_key] = std::make_unique<detail::DeletedObj>(old_key);
        map_it_.reset();
    }

public: // IDson
	void clear() override
	{
		map_.clear();
		map_it_.reset();
		key_ = 0;
		// header_.data_size_ = 0;
		// header_.data_type_ = detail::types_map<detail::DsonContainer>::value;
	}

	void set_key(const Key key) override
	{
		key_ = key;
	}

	bool has(const Key key) override
	{
		return map_.end() != map_.find(key);
	}

	IObjView * self() override
	{
		return this;
	}

	void erase(const Key key) override
	{
		map_[key] = std::make_unique<detail::DeletedObj>(key);
	}

	bool find(const Key key, const std::function<void(IObjView *)> & fun) override
	{
		auto it = map_.find(key);
		if (it == map_.end())
			return false;
		fun(it->second.get());
		return true;
	}

	std::unique_ptr<IDson> move_self() override
	{
		return std::make_unique<NewDson>(std::move(*this));
	}

	// Удаление существующих ключей должно быть реализовано у потомков выше по колстэку
	// в файле и буффере через прометку NoType и уменьшение размера
	void emplace_dson(std::unique_ptr<IObjView> obj) override
	{
		const auto id = obj->obj_key();
		map_[id] = std::move(obj);
		map_it_.reset();
	}

	void private_iterate(const std::function<void(std::unique_ptr<IObjView>)> & callback) override
	{
		for (auto & it : map_)
		{
			if (it.second->data_type() == detail::types_map<detail::NoType>::value)
				continue;
			callback(std::make_unique<detail::RawIObjViewPtr>(it.second.get()));
		}
	}

public: // IObjView
	std::int32_t data_size() override
	{
		std::int32_t size{0};
		for (auto & it : map_)
		{
			size += it.second->all_size();
		}
		return size;
	}

	Key obj_key() override
	{
		return key_;
	}

	result_t upload_to(const std::int32_t at, detail::IUploader & uploader) override
	{
		if (at < detail::header_size)
		{
			map_it_.reset();
			auto buf = uploader.upload_with_uploader_buf(detail::header_size);
			if (!buf)
				return eNoMemory;
			DsonHeader header;
			header.key_ = key_;
			header.data_type_ = detail::types_map<detail::DsonContainer>::value;
			header.data_size_ = data_size();
			detail::put(buf, header);
			// Полный целевой размер известен в uploader и он сам вернёт okInProcess | okReady
			return uploader.set_uploaded(detail::header_size);
			// return header_.data_size_ > 0 ? okInProcess : okReady;
		}

		if (map_.empty())
		{
			map_it_from_ = map_it_to_ = detail::header_size;
			return eFailMoreThanIHave;
		}

		if (!map_it_ || map_it_from_ > at)
		{ // Если выгрузка перезапущена, то итератор в исходное
			map_it_ = map_.begin();
			map_it_from_ = detail::header_size;
			map_it_to_ = map_it_from_ + map_it_.value()->second->all_size();
		}

		if (map_it_.value() == map_.end())
			return eFailMoreThanIHave;

		while (at >= map_it_to_)
		{ // Перемотка в позицию at
			map_it_ = ++(map_it_.value());
			if (map_it_.value() == map_.end())
			{ // был запрошен at дальше чем есть в map => это уже диск/буффер
				return eFailMoreThanIHave;
			}
			map_it_from_ = map_it_to_;
			map_it_to_ += map_it_.value()->second->all_size();
		}

		return map_it_.value()->second->upload_to(at - map_it_from_, uploader);
	}

protected:
	void move_from_other(NewDson && other)
	{
		key_ = other.key_;
		map_ = std::move(other.map_);
		other.map_it_ = {};
		map_it_ = {};
	}

protected:
	// DsonHeader header_; добавленные объекты могут менять размер - например Dson
	// Поэтому header готовится каждый раз когда нужен
	Key key_;
	using Map = std::map<Key, std::unique_ptr<IObjView>>;
	Map map_;
	// Суммарный объём объектов в map_
	// std::int32_t map_data_size_{0}; нельзя использовать так как data_size() и доступ к header_ ломают эту логику

	// Для ускорения upload_to() запоминаем последнюю итерацию
	std::optional<Map::iterator> map_it_{};
	std::int32_t map_it_from_{0};
	std::int32_t map_it_to_{0};
}; // NewDson

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_NEW_DSON_H
