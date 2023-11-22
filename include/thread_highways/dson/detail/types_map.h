/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DETAIL_TYPES_MAP_H
#define THREADS_HIGHWAYS_DSON_DETAIL_TYPES_MAP_H

#include <thread_highways/dson/detail/obj_view.h>
#include <thread_highways/tools/buf_uchar.h>

#include <cstdint>
#include <string>
#include <vector>

namespace hi
{
namespace dson
{

// Адаптер для извлечения данных (строки/вектора байт) без копирования
//    struct VectorNoCopyAdapter
//    {
//        using value_type = std::uint8_t;
//        VectorNoCopyAdapter() = default;
//        VectorNoCopyAdapter(const value_type* data, std::size_t size)
//        : data_{data}
//        , size_{size}
//        {}
//        VectorNoCopyAdapter(const VectorNoCopyAdapter& other)
//        {
//            data_ = other.data_;
//            size_ = other.size_;
//        }
//        VectorNoCopyAdapter(VectorNoCopyAdapter&& other)
//        {
//            data_ = other.data_;
//            size_ = other.size_;
//        }
//        VectorNoCopyAdapter& operator=(const VectorNoCopyAdapter& other)
//        {
//            if (&other == this) return *this;
//            data_ = other.data_;
//            size_ = other.size_;
//            return *this;
//        }
//        VectorNoCopyAdapter& operator=(VectorNoCopyAdapter&& other)
//        {
//            if (&other == this) return *this;
//            data_ = other.data_;
//            size_ = other.size_;
//            return *this;
//        }
//        ~VectorNoCopyAdapter(){}
//        const value_type* data_;
//        std::size_t size_;
//    };

namespace detail
{

using TypeMarker = std::int32_t;

template <int N>
struct marker_id
{
	static TypeMarker const value = N;
};

template <typename T>
struct marker_type
{
	typedef T type;
};

template <typename T, int N>
struct register_id
	: marker_id<N>
	, marker_type<T>
{
private:
	friend marker_type<T> marked_id(marker_id<N>)
	{
		return marker_type<T>();
	}
};

template <typename T>
struct types_map;

/**
 * Заполнение справочника идентификаторов типов данных.
 * Для отправки через сеть типам данных назначается std::int32_t идентификатор.
 * (знаковый для совместимости с другими языками)
 * Данный справочник нужен в качестве реестра вроде Enum.
 * Enum не используется чтобы дать возможность дополнять справочник
 * пользовательскими типами данных в разных местах программы.
 */

// Структура - метка ошибки
struct NoType
{
};
template <>
struct types_map<NoType> : register_id<NoType, 0>
{
};

// Структура - метка контейнера, содержащего другие объекты
class DsonContainer
{
};
template <>
struct types_map<DsonContainer> : register_id<DsonContainer, 1>
{
};

template <>
struct types_map<bool> : register_id<bool, 2>
{
};

template <>
struct types_map<std::int8_t> : register_id<std::int8_t, 3>
{
};

template <>
struct types_map<std::uint8_t> : register_id<std::uint8_t, 4>
{
};

template <>
struct types_map<std::int16_t> : register_id<std::int16_t, 5>
{
};

template <>
struct types_map<std::uint16_t> : register_id<std::uint16_t, 6>
{
};

template <>
struct types_map<std::int32_t> : register_id<std::int32_t, 7>
{
};

template <>
struct types_map<std::uint32_t> : register_id<std::uint32_t, 8>
{
};

template <>
struct types_map<std::int64_t> : register_id<std::int64_t, 9>
{
};

template <>
struct types_map<std::uint64_t> : register_id<std::uint64_t, 10>
{
};

// bool должен быть первым ID из чисел, double должен быть последним ID из чисел так как в коде
// if (type >= detail::types_map<bool>::value && type <= detail::types_map<double>::value)
template <>
struct types_map<double> : register_id<double, 11>
{
};

template <>
struct types_map<std::string> : register_id<std::string, 12>
{
};

template <>
struct types_map<std::vector<std::int8_t>> : register_id<std::vector<std::int8_t>, 13>
{
};

template <>
struct types_map<std::vector<std::uint8_t>> : register_id<std::vector<std::uint8_t>, 14>
{
};

template <>
struct types_map<std::vector<std::int16_t>> : register_id<std::vector<std::int16_t>, 15>
{
};

template <>
struct types_map<std::vector<std::uint16_t>> : register_id<std::vector<std::uint16_t>, 16>
{
};

template <>
struct types_map<std::vector<std::int32_t>> : register_id<std::vector<std::int32_t>, 17>
{
};

template <>
struct types_map<std::vector<std::uint32_t>> : register_id<std::vector<std::uint32_t>, 18>
{
};

template <>
struct types_map<std::vector<std::int64_t>> : register_id<std::vector<std::int64_t>, 19>
{
};

template <>
struct types_map<std::vector<std::uint64_t>> : register_id<std::vector<std::uint64_t>, 20>
{
};

template <>
struct types_map<std::vector<double>> : register_id<std::vector<double>, 21>
{
};

template <>
struct types_map<BufUCharView> : register_id<BufUCharView, 22>
{
};

template <>
struct types_map<BufUChar> : register_id<BufUChar, 23>
{
};
const std::int32_t last_type_id{23};

inline bool is_ok_header(const DsonHeader & header)
{
	if (header.data_size_ < 0)
		return false;
	if (header.data_type_ <= 0 || header.data_type_ > last_type_id)
		return false;
	return true;
}

inline bool is_dson_header(const DsonHeader & header)
{
	if (header.data_size_ < 0)
		return false;
	if (header.data_type_ != detail::types_map<detail::DsonContainer>::value)
		return false;
	return true;
}
// Заглушка вместо объекта который удалили или переименовали (изменили ключ)
// Служит для того чтобы старая реализация не была скопирована
struct DeletedObj : public IObjView
{
	DeletedObj(Key key)
		: key_{key}
	{
	}

	// Тип хранимого объекта
	std::int32_t data_type() override
	{
		return types_map<NoType>::value;
	}

	std::int32_t data_size() override
	{
		return 0;
	}

	std::int32_t all_size() override
	{
		return 0;
	}

	Key obj_key() override
	{
		return key_;
	}

	IObjView * self() override
	{
		return this;
	}

	Key key_;
};

} // namespace detail

// грузи хидер по месту применения и дальше с параметром get
// template<typename T>
// inline bool get_using_header(const char* &, T& )
//{
//    assert (false);
//}

// template<>
// inline bool get_using_header<bool>(const char* &header, bool& val)
//{
//     assert (false);
// }

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DETAIL_TYPES_MAP_H
