/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_H
#define THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_H

#include <thread_highways/dson/dson_from_file.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace hi
{
namespace dson
{

/*
	Важно: есть возможность открыть только файл с диске == всё можно представить через оффсет в файле.
	Изменения попадают в changes_, и при сохранении в файл
	 => нет нужды анализировать иные механизмы хранения Dson объектов (NewDson map и т.п.).

	В GUI необходимо ходить по Dson как по каталогам диска, сортировать.
	GUI хочет получать DsonObj по operator[index], с разными сортировками
	Надо будет хранить текущий path_vector<Key> по пути иерархии папок до текущей

	GUI хочет менять на любом уровне значения объектов, добавлять/удалять объекты
	 - если это сразу в файле отражать, то будут тормоза => контроллер хранит эти изменения
	 пока save не сбросит всё на диск. Хранить будет как map<path_vector<Key>, DsonObj>.
	 Если произошло удаление, то DsonObj будет типа NoType.
	 DsonObj может быть большой 100Мб бинарь с диска - хранить как путь
*/
class DsonEditController : public DsonFromFile
{
protected:
	struct PathRecord
	{
		NewDson * ref_;
		std::size_t index_;
	};
	using Path = std::deque<PathRecord>;

public:
	DsonEditController()
	{
		open_folder_local(this, 0);
	}

	DsonEditController(std::string path)
		: DsonFromFile{std::move(path)}
	{
		open_folder_local(this, 0);
	}

	~DsonEditController()
	{
        clear();
	}

	void create_default(const std::string & folder)
	{
		temp_file_ = folder + "/default_" + std::to_string(steady_clock_now().count()) + ".dson";
		HI_ASSERT(okCreatedNew == DsonFromFile::create(temp_file_));
		reload_all();
	}

    void clear() override
    {
        close();
        DsonFromFile::clear();
    }

	void open(std::string path) override
	{
		close();
		DsonFromFile::open(std::move(path));
		reload_all();
	}

	void save() override
	{
		DsonFromFile::save();
		// Вся иерархия теперь сломана, надо начинать с начала
		reload_all();
	}

	void save_as(std::string path) override
	{
		DsonFromFile::save_as(std::move(path));
		// Вся иерархия теперь сломана, надо начинать с начала
		reload_all();
		if (!temp_file_.empty())
		{
			std::remove(temp_file_.c_str());
			temp_file_.clear();
		}
	}

	// количество элементов в текущем открытом Dson
	std::size_t items_on_level()
	{
		return current_folder_map_.size();
	}

	// Получить уникальный для уровня ключ
	Key generate_unique_key()
	{
		Key key = 1;
		while (current_folder_map_.find(key) != current_folder_map_.end())
		{
			++key;
		}
		return key;
	}

	void emplace_dson(std::unique_ptr<IObjView> obj) override
	{
		if (this == current_path_.back().ref_)
		{
			DsonFromFile::emplace_dson(std::move(obj));
		}
		else
		{
			current_path_.back().ref_->emplace_dson(std::move(obj));
		}

		reload_folder();
	}

	template <typename T>
	void emplace(T val)
	{
		emplace_dson(val.move_self());
	}

	template <typename K, typename T>
	void emplace(const K key, T val)
	{
		const auto type_id = detail::types_map<T>::value;
		HI_ASSERT(type_id != detail::types_map<detail::NoType>::value);

		auto view = std::make_unique<DsonObj<T>>(static_cast<Key>(key), std::move(val));
		HI_ASSERT(!!view);
		emplace_dson(std::move(view));
	}

	// Метод позволяет из GUI указать в какой целевой тип преобразовать std::int64_t
	template <typename K>
	void emplace(const K key, std::int64_t value, std::int32_t data_type = detail::types_map<std::int64_t>::value)
	{
		HI_ASSERT(data_type > detail::types_map<detail::NoType>::value && data_type < detail::last_type_id);
		std::unique_ptr<IObjView> obj;
		switch (data_type)
		{
		case detail::types_map<bool>::value:
			{
				HI_ASSERT(value == 0 || value == 1);
				bool val = value == 1;
				obj = std::make_unique<DsonObj<bool>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::int8_t>::value:
			{
				HI_ASSERT(
					value >= std::numeric_limits<std::int8_t>::lowest()
					&& value <= std::numeric_limits<std::int8_t>::max());
				std::int8_t val = static_cast<std::int8_t>(value);
				obj = std::make_unique<DsonObj<std::int8_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::uint8_t>::value:
			{
				HI_ASSERT(value >= 0u && value <= static_cast<std::int64_t>(std::numeric_limits<std::uint8_t>::max()));
				std::uint8_t val = static_cast<std::uint8_t>(value);
				obj = std::make_unique<DsonObj<std::uint8_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::int16_t>::value:
			{
				HI_ASSERT(
					value >= std::numeric_limits<std::int16_t>::lowest()
					&& value <= std::numeric_limits<std::int16_t>::max());
				std::int16_t val = static_cast<std::int16_t>(value);
				obj = std::make_unique<DsonObj<std::int16_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::uint16_t>::value:
			{
				HI_ASSERT(value >= 0u && value <= static_cast<std::int64_t>(std::numeric_limits<std::uint16_t>::max()));
				std::uint16_t val = static_cast<std::uint16_t>(value);
				obj = std::make_unique<DsonObj<std::uint16_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::int32_t>::value:
			{
				HI_ASSERT(
					value >= std::numeric_limits<std::int32_t>::lowest()
					&& value <= std::numeric_limits<std::int32_t>::max());
				std::int32_t val = static_cast<std::int32_t>(value);
				obj = std::make_unique<DsonObj<std::int32_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::uint32_t>::value:
			{
				HI_ASSERT(value >= 0u && value <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()));
				std::uint32_t val = static_cast<std::uint32_t>(value);
				obj = std::make_unique<DsonObj<std::uint32_t>>(static_cast<Key>(key), val);
				break;
			}
		case detail::types_map<std::int64_t>::value:
			{
				obj = std::make_unique<DsonObj<std::int64_t>>(static_cast<Key>(key), value);
				break;
			}
		case detail::types_map<std::uint64_t>::value:
			{
				HI_ASSERT(value >= 0u);
				std::uint64_t val = static_cast<std::uint64_t>(value);
				obj = std::make_unique<DsonObj<std::uint64_t>>(static_cast<Key>(key), val);
				break;
			}

		default:
			break;
		}

		HI_ASSERT(!!obj);
		emplace_dson(std::move(obj));
	}

	template <typename K>
	void emplace_serializable(const K key, const Serializable & val)
	{
		emplace<K, std::string>(key, val.serialize());
	}

	void rename_item(const Key old_key, const Key new_key) override
	{
		NewDson::rename_item(old_key, new_key);
		reload_folder();
	}

	bool find(const Key key, const std::function<void(IObjView *)> & fun) override
	{
		auto it = current_folder_map_.find(key);
		if (it == current_folder_map_.end())
			return false;
		fun(it->second->self());
		return true;
	}

	IObjView * operator[](const std::size_t index) const
	{
		if (index > current_folder_map_.size())
			return nullptr;
		auto it = current_folder_map_.begin(); // 0
		std::advance(it, index);
		return it->second.get();
	}

	void get_header_at(const std::size_t index, DsonHeader & header)
	{
		IObjView * ptr = operator[](index);
		if (!ptr)
		{
			throw Exception("wrong index: " + std::to_string(index), __FILE__, __LINE__);
		}
		IObjView * obj = ptr->self();
		header.key_ = obj->obj_key();
		header.data_size_ = obj->data_size();
		header.data_type_ = obj->data_type();
	}

	template <typename T>
	void get_data_at(const std::size_t index, const std::int32_t key, T & result)
	{
		IObjView * ptr = operator[](index);
		HI_ASSERT(!!ptr && ptr->obj_key() == key);

		IObjView * view = ptr->self();
		HI_ASSERT(!!view);
		if (view->data_type() != detail::types_map<T>::value)
		{
			throw Exception(
				"view->data_type(){" + std::to_string(view->data_type()) + " != detail::types_map<T>::value{"
					+ std::to_string(detail::types_map<T>::value),
				__FILE__,
				__LINE__);
		}

		{
			auto ptr = dynamic_cast<DsonObj<T> *>(view);
			if (ptr)
			{
				ptr->get(result);
				return;
			}
		}

		{
			auto ptr = dynamic_cast<DsonObjView *>(view);
			if (ptr)
			{
				ptr->get<T>(result);
				return;
			}
		}

        HI_ASSERT(cast<T>(result, view));
	}

	void get_data_at(const std::size_t index, const std::int32_t key, BufUCharView & result)
	{
		IObjView * ptr = operator[](index);
		HI_ASSERT(!!ptr && ptr->obj_key() == key);

		IObjView * view = ptr->self();
        HI_ASSERT(!!view && view->buf_view_skip_header(result));
	}

	void get_data_at(const std::size_t index, const std::int32_t key, std::string_view & result)
	{
		BufUCharView buf;
		get_data_at(index, key, buf);
		result = std::string_view{reinterpret_cast<const char *>(buf.data()), static_cast<std::size_t>(buf.size())};
	}

	void get_data_at(const std::size_t index, const std::int32_t key, std::int64_t & result)
	{
		IObjView * ptr = operator[](index);
		HI_ASSERT(!!ptr && ptr->obj_key() == key);

		IObjView * view = ptr->self();
		HI_ASSERT(!!view && cast_as_int64_t(result, view));
	}

	void open_folder(const std::size_t index)
	{
		HI_ASSERT(index < current_folder_map_.size());
		// if (index >= current_folder_map_.size()) return eWrongParams;
		auto it = current_folder_map_.begin(); // 0
		std::advance(it, index);
		HI_ASSERT(it->second->self()->data_type() == detail::types_map<detail::DsonContainer>::value);
		// if (it->second->self()->data_type() !=  detail::types_map<detail::DsonContainer>::value) return
		// eFailWrongHeader;
		auto dson = dynamic_cast<NewDson *>(it->second->self());
		HI_ASSERT(dson);
		// if (!dson) return eFailState;
		//  Считаем что начали редактировать и теперь этот открываемый Dson
		//  Если ещё не перенесли в DsonNew, то переносим
		current_path_.back().ref_->try_emplace(std::move(it->second));
		open_folder_local(dson, index);
	}

	/**
	 * @brief close_folder
	 * Закрывает текущий открытый Dson
	 * @return индекс на котором был этот Dson на уровне выше чтобы GUI мог правильно подсветить
	 */
    std::size_t close_folder()
	{
		if (current_path_.size() < 2u)
            return 0u; // нельзя закрыть root dson, остаёмся на 0
        const auto out_index = current_path_.back().index_;
		current_path_.pop_back(); // закрыли текущий
		reload_folder();
        return out_index;
	}

    bool is_top_folder_level()
    {
        return current_path_.size() < 2u;
    }

    void folder_set_key(const Key key)
    {
        if (is_top_folder_level())
        {
            set_key(key);
            return;
        }

        current_path_[current_path_.size() - 1].ref_->rename_item(current_path_.back().ref_->obj_key(), key);
    }

	void delete_at(const std::size_t index)
	{
		if (index >= current_folder_map_.size())
			return;
		auto it = current_folder_map_.begin(); // 0
		std::advance(it, index);
		current_path_.back().ref_->erase(it->second->obj_key());
		reload_folder();
	}

    void erase(const Key key) override
    {
        DsonFromFile::erase(key);
        reload_folder();
    }

    /**
     * @brief dictionary_filter
     * @param filter фильтр на имя
     * @param current_key элемент с текущим ключом не отфильтровывается
     * @return  размер справочника
     */
    std::size_t dictionary_filter(std::string filter, const Key current_key)
    {
        auto& dictionary = get_dictionary();
        HI_ASSERT(dictionary.obj_key() == static_cast<Key>(SystemKey::Dictionary));
        if (dictionary_filter_ == filter && dictionary_cache_.size())
        {
            return dictionary_cache_.size();
        }
        dictionary_filter_ = filter;
        dictionary_cache_.clear();

        dictionary.private_iterate([&](std::unique_ptr<IObjView> item)
        {
            std::string val;
            if (!cast<std::string>(val, item.get())) return;
            const auto key = item->obj_key();
            if (current_key != key && !dictionary_filter_.empty() && val.find(dictionary_filter_) == std::string::npos) return;
            dictionary_cache_.emplace(key, std::move(val));
        });
        return dictionary_cache_.size();
    }

    void get_dictionary_item_at(const std::size_t index, Key& key, std::string& name)
    {
        HI_ASSERT(get_dictionary().obj_key() == static_cast<Key>(SystemKey::Dictionary));
        HI_ASSERT(index < dictionary_cache_.size());
        auto it = dictionary_cache_.begin(); // 0
        std::advance(it, index);
        key = it->first;
        name = it->second;
    }

    std::optional<std::string> get_dictionary_name(const Key key)
    {
        HI_ASSERT(get_dictionary().obj_key() == static_cast<Key>(SystemKey::Dictionary));
        if (auto it = dictionary_cache_.find(key); it != dictionary_cache_.end())
        {
            return it->second;
        }
        return {};
    }

    void set_dictionary(const Key key, std::string name)
    {
        auto& dictionary = get_dictionary();
        dictionary_cache_[key] = name;
        dictionary.emplace(key, std::move(name));
    }

protected:
	void close()
	{
        dictionary_.reset();
		current_folder_map_.clear();
		if (!temp_file_.empty())
		{
			std::remove(temp_file_.c_str());
		}
		temp_file_.clear();
	}

	void open_folder_local(NewDson * dson, std::size_t index)
	{
		current_path_.emplace_back(PathRecord{dson, index});
		reload_folder();
	}

	void reload_folder()
	{
		current_folder_map_.clear();
		current_path_.back().ref_->private_iterate(
			[&](std::unique_ptr<IObjView> ptr)
			{
				auto key = ptr->obj_key();
                if (key < 0) return ; // SystemKey
				current_folder_map_.emplace(key, std::move(ptr));
			});
	}

	void reload_all()
	{
		Path current_path;
		std::swap(current_path, current_path_);
		open_folder_local(this, 0);
	}

    IDson& get_dictionary()
    {
        const auto idson = [&]()->IDson&
        {
            HI_ASSERT(dictionary_);
            auto real_view = dictionary_->self();
            auto ptr = dynamic_cast<IDson *>(real_view);
            HI_ASSERT(!!ptr);
            return *ptr;
        };
        if (dictionary_) return idson();
        const auto dict_key = static_cast<Key>(SystemKey::Dictionary);
        const auto dict_locator = [&](std::unique_ptr<IObjView> ptr)
            {
                if (dict_key != ptr->obj_key()) return;
                auto real_view = ptr->self();
                if (real_view->data_type() != detail::types_map<detail::DsonContainer>::value)
                {
                    return;
                }
                dictionary_ = std::move(ptr);
            };

        const auto load_cache = [&]()->IDson&
        {
            auto& re = idson();
            re.private_iterate([&](std::unique_ptr<IObjView> ptr)
            {
                std::string name;
                if (cast(name, ptr.get()))
                {
                    dictionary_cache_.emplace(ptr->obj_key(), std::move(name));
                }
            });
            return re;
        };

        DsonFromFile::private_iterate(dict_locator);
        if (dictionary_) return load_cache();

        // Нет ещё справочника => добавляю
        NewDson dson;
        dson.set_key_cast(SystemKey::Dictionary);
        NewDson::emplace_dson(dson.move_self());
        NewDson::private_iterate(dict_locator);
        return idson();
    }

protected:
	Path current_path_;

	// Map для быстрого поиска, понятной сортировки и гарантии уникальности
	std::map<Key, std::unique_ptr<IObjView>> current_folder_map_;

	// Если был создан временный файл
	std::string temp_file_;

    // Dictionary filter
    std::string dictionary_filter_;
    std::map<Key, std::string> dictionary_cache_;
    std::unique_ptr<IObjView> dictionary_;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_H
