/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_IDSON_H
#define THREADS_HIGHWAYS_DSON_IDSON_H

#include <thread_highways/dson/detail/dson_obj.h>
#include <thread_highways/dson/detail/i_uploader.h>
#include <thread_highways/dson/i_has_route_id.h>
#include <thread_highways/tools/result_code.h>

#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits> // enable_if

namespace hi
{
namespace dson
{

template <typename T>
inline bool cast(T & result, IObjView * view)
{
	auto real_view = view->self();
	if (real_view->data_type() != detail::types_map<T>::value)
		return false;
	if (auto ptr = dynamic_cast<DsonObj<T> *>(real_view); ptr)
	{
		ptr->get(result);
		return true;
	}
	if (auto ptr = dynamic_cast<DsonObjView *>(real_view); ptr)
	{
		ptr->get<T>(result);
		return true;
	}

	BufUCharView buf;
	if (!real_view->buf_view(buf))
		return false;
	detail::GetObj<T, detail::IsDsonNumber<T>::result>::get(reinterpret_cast<const char *>(buf.data()), result);
	return true;
}

// Специальный вариант позволяет считать любое число в int64
inline bool cast_as_int64_t(std::int64_t & result, IObjView * view)
{
	auto real_view = view->self();
	auto type = view->data_type();
	// if (real_view->data_type() != detail::types_map<T>::value) return false;
	switch (type)
	{
	case detail::types_map<bool>::value:
		{
			bool val;
			if (cast<bool>(val, real_view))
			{
				result = val ? 1 : 0;
				return true;
			}
			break;
		}
	case detail::types_map<std::int8_t>::value:
		{
			std::int8_t val;
			if (cast<std::int8_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::uint8_t>::value:
		{
			std::uint8_t val;
			if (cast<std::uint8_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::int16_t>::value:
		{
			std::int16_t val;
			if (cast<std::int16_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::uint16_t>::value:
		{
			std::uint16_t val;
			if (cast<std::uint16_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::int32_t>::value:
		{
			std::int32_t val;
			if (cast<std::int32_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::uint32_t>::value:
		{
			std::uint32_t val;
			if (cast<std::uint32_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::int64_t>::value:
		{
			std::int64_t val;
			if (cast<std::int64_t>(val, real_view))
			{
				result = val;
				return true;
			}
			break;
		}
	case detail::types_map<std::uint64_t>::value:
		{
			std::uint64_t val;
			if (cast<std::uint64_t>(val, real_view) && std::numeric_limits<std::int64_t>::max() < val)
			{
				result = val;
				return true;
			}
			break;
		}
	default:
		break;
	}
	return false;
}

// namespace detail {

// template<typename T>
// typename std::enable_if<is_dson_number<T>(), bool>::type
// obj_view_to_type(IObjView* view, T& out)
//{
//     std::int32_t buf_size{0};
//     const char* buf = view->at_out(detail::header_size, buf_size);
//     if (!buf || buf_size < static_cast<std::int32_t>(sizeof(T))) return false;
//     detail::get(buf, out);
//     return true;
// }

// template<typename T>
// typename std::enable_if<!is_dson_number<T>(), bool>::type
// obj_view_to_type(IObjView* view, T& out)
//{
//     std::int32_t buf_size{0};
//     const char* buf = view->at_out(0, buf_size);
//     if (!buf || buf_size <= detail::header_size) return false;
//     detail::Header header;
//     detail::get(buf, header);
//     detail::get_array(buf, header.data_size_, out);
//     return true;
// }

//} // namespace detail

struct Serializable
{
	virtual ~Serializable() = default;
	virtual std::string serialize() const = 0;
	virtual bool deserialize(const char * buf, std::int32_t size) = 0;
};

class IDson
	: public IObjView
	, public IHasRouteID
{
public:
	enum class Result
	{
		// Возникли ошибки
		Error,

		// Готово, можно идти дальше
		Ready,

		// Работа в процессе, но всё равно можно идти дальше
		InProcess
	};

	virtual ~IDson() = default;

	virtual void clear() = 0;

	// virtual Key get_key() = 0; == obj_key()
	virtual void set_key(const Key key) = 0;

	template <typename K>
	void set_key_cast(const K key)
	{
		set_key(static_cast<Key>(key));
	}

	virtual bool has(const Key key) = 0;
	virtual void erase(const Key)
	{
	}

	// Поиск с обработкой найденного
	// IObjView* всегда ненулевой, указатель для удобства каста
	virtual bool find(const Key key, const std::function<void(IObjView *)> &) = 0;

	// Dson потомки умеют смувать своё содержимое в unique_ptr для сохранения в составе других Dson
	virtual std::unique_ptr<IDson> move_self() = 0;

	virtual void emplace_dson(std::unique_ptr<IObjView> obj) = 0;

	// Добавление Dson
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

	template <typename K>
	void emplace_serializable(const K key, const Serializable & val)
	{
		emplace<K, std::string>(key, val.serialize());
	}

	// Извлечение простого объекта (число/вектор/строка)
	template <typename K, typename T>
	bool get(const K key, T & result)
	{
		const auto type_id = detail::types_map<T>::value;
		if (type_id == detail::types_map<detail::NoType>::value)
		{
			return false;
		}

		bool uploaded{false};
		const auto casted_key = static_cast<Key>(key);
		bool found = find(
			casted_key,
			[&](IObjView * view)
			{
				uploaded = cast<T>(result, view);
			});

		return found && uploaded;
	}

	template <typename K>
	bool get(const K key, BufUCharView & result)
	{
		bool uploaded{false};
		const auto casted_key = static_cast<Key>(key);
		bool found = find(
			casted_key,
			[&](IObjView * view)
			{
				uploaded = view->buf_view(result);
			});

		return found && uploaded;
	}

	template <typename K>
	bool get(const K key, std::string_view & result)
	{
		bool uploaded{false};
		const auto casted_key = static_cast<Key>(key);
		bool found = find(
			casted_key,
			[&](IObjView * view)
			{
				BufUCharView buf;
				uploaded = view->buf_view(buf);
				if (uploaded)
				{
					result = std::string_view{
						reinterpret_cast<const char *>(buf.data()),
						static_cast<std::size_t>(buf.size())};
				}
			});

		return found && uploaded;
	}

	template <typename K>
	bool get(const K key, std::int64_t & result)
	{
		bool uploaded{false};
		const auto casted_key = static_cast<Key>(key);
		bool found = find(
			casted_key,
			[&](IObjView * view)
			{
				uploaded = cast_as_int64_t(result, view);
			});

		return found && uploaded;
	}

	template <typename K>
	bool get_serializable(const K key, Serializable & val)
	{
		bool uploaded{false};
		const auto casted_key = static_cast<Key>(key);
		bool found = find(
			casted_key,
			[&](IObjView * view)
			{
				BufUCharView buf;
				if (!view->buf_view(buf))
					return;
				uploaded =
					val.deserialize(reinterpret_cast<const char *>(buf.data()), static_cast<std::int32_t>(buf.size()));
			});

		return found && uploaded;
	}

	// Обход объектов, в колбэк будут переданы хидера один за другим
	// virtual result_t iterate(const std::function<void(const DsonHeader&)>&) = 0;

	// Обход объектов включая объекты внутри вложенных Dson-ов,
	// в колбэк будут переданы хидера один за другим + путь до каждого.
	// Вектор std::vector<Key> начинается с ключа корневого Dson и далее ключи для всех дочерних Dson
	// , ключ объекта из хидера не входит.
	// virtual void iterate_all(const std::function<void(std::vector<Key>, detail::Header&)>&) = 0;

	// IObjView
	std::int32_t data_type() override
	{
		return detail::types_map<detail::DsonContainer>::value;
	}

	// IHasRouteID
	bool get_route_id(RouteID & route_id) override
	{
		return get(static_cast<Key>(SystemKey::RouteID), route_id);
	}

	void set_route_id(RouteID route_id)
	{
		emplace(SystemKey::RouteID, route_id);
	}

	friend class DsonEditController;

protected:
	/**
	 * @brief private_iterate
	 * В DsonEditController необходимо хранить ссылки на объекты и при этом
	 * время жизни объектов контроллируется этим контроллером, поэтому
	 * возможно сохранять unique_ptr.
	 *
	 * Эти возвращаемые unique_ptr могут стать невалидными при изменении Dson,
	 * поэтому рекомендуется только для внутреннего использования.
	 */
	virtual void private_iterate(const std::function<void(std::unique_ptr<IObjView>)> &) = 0;
};

class ICanUploadIntoDson : public detail::IUploader
{
public:
	// Установка заголовка и аллокация ресурсов под приём данных
	virtual result_t set_header(const DsonHeader & header) = 0;

	// Максимальный объём данных исходя из возможностей и потребностей в header
	virtual std::int32_t max_chunk_size() = 0;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_IDSON_H
