/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DETAIL_HEADER_H
#define THREADS_HIGHWAYS_DSON_DETAIL_HEADER_H

#include <thread_highways/tools/buf_uchar.h>
#include <thread_highways/tools/fd_write_read.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring> // memcpy
#include <limits>
#include <new>
#include <string>
#include <vector>

#ifdef __GNUC__
#	define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#	define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

namespace hi
{
namespace dson
{

using Key = std::int32_t;

enum class SystemKey : std::int32_t
{
    Dictionary = -5,
    Deleted = -4,
    RouteID = -3,
    Error = -2,

    LastInEnum = -1, // be shure this last
};

// Reserved keys [-999 .. 1]
inline constexpr Key min_positive_user_key{0};
inline constexpr Key max_negative_user_key{-1000};

inline bool is_system_key(Key key)
{
	return key >= static_cast<std::int32_t>(SystemKey::RouteID)
		&& key <= static_cast<std::int32_t>(SystemKey::LastInEnum);
}

inline SystemKey to_system_key(Key key)
{
	return static_cast<SystemKey>(key);
}

/*
  Заголовок буфера объекта, хранящегося в Dson.
  Описывает содержимое буфера.
  Аллоцированный буфер всегда начинается с заголовка
  (размер буфера = sizeof(Header) + размер хранимых данных)

  Юзерская версия Header без PACK
*/
struct DsonHeader
{
	// Ключ для поиска/идентификации данных
	Key key_; // [0]
	// Размер данных
	std::int32_t data_size_; // [1]
	// Тип данных
	std::int32_t data_type_; // [2]
};

namespace detail
{

/*
  Заголовок буфера объекта, хранящегося в Dson.
  Описывает содержимое буфера.
  Аллоцированный буфер всегда начинается с заголовка
  (размер буфера = sizeof(Header) + размер хранимых данных)

  PACK для того чтобы компилятор не вздумал менять размер структуры
*/
PACK(struct Header {
	// Ключ для поиска/идентификации данных
	Key key_; // [0]
	// Размер данных
	std::int32_t data_size_; // [1]
	// Тип данных
	std::int32_t data_type_; // [2]
});

inline constexpr std::uint32_t mark_host_order{1};
inline const std::uint32_t mark_network_order{htonl(mark_host_order)};

//  Так как большинство используемых машин в LittleEndian, то и я буду класть в LittleEndian
// == тормоза преобразований только на редких BigEndian машинах
inline const bool must_reorder =
	(mark_host_order == mark_network_order); // network_oder == BigEndian => reorder to LittleEndian

inline constexpr std::int32_t header_size{sizeof(Header)};

inline Header * as_header(char * header)
{
	return std::launder(reinterpret_cast<Header *>(header));
}

inline const Header * as_header(const char * header)
{
	return std::launder(reinterpret_cast<const Header *>(header));
}

inline void put_reordered1(char * to, const char * from)
{
	to[0] = from[0];
}

inline void put_reordered2(char * to, const char * from)
{
	assert(false);
	to[0] = from[0];
	to[1] = from[1];
}

inline void put_reordered4(char * to, const char * from)
{
	assert(false);
	to[0] = from[3];
	to[1] = from[2];
	to[2] = from[1];
	to[3] = from[0];
}

inline void put_reordered8(char * to, const char * from)
{
	to[0] = from[7];
	to[1] = from[6];
	to[2] = from[5];
	to[3] = from[4];
	to[4] = from[3];
	to[5] = from[2];
	to[6] = from[1];
	to[7] = from[0];
}

inline void get_reordered1(const char * from, char * to)
{
	to[0] = from[1];
}

inline void get_reordered2(const char * from, char * to)
{
	assert(false);
	to[0] = from[1];
	to[1] = from[0];
}

inline void get_reordered4(const char * from, char * to)
{
	assert(false);
	to[0] = from[3];
	to[1] = from[2];
	to[2] = from[1];
	to[3] = from[0];
}

inline void get_reordered8(const char * from, char * to)
{
	to[0] = from[7];
	to[1] = from[6];
	to[2] = from[5];
	to[3] = from[4];
	to[4] = from[3];
	to[5] = from[2];
	to[6] = from[1];
	to[7] = from[0];
}

enum class FloatPointType : std::uint8_t
{
	// Normal negative number
	NormalNegative,
	// Normal positive number
	NormalPositive,
	// Not-a-number (NaN) value
	NaN,
	// Negative infinity
	InfNegative,
	// Positive infinity.
	InfPositive
};

inline FloatPointType type(double val)
{
	if (std::isnan(val))
	{
		return FloatPointType::NaN;
	}
	if (std::isinf(val))
	{
		return (val < 0.0) ? FloatPointType::InfNegative : FloatPointType::InfPositive;
	}
	return (val < 0.0) ? FloatPointType::NormalNegative : FloatPointType::NormalPositive;
}

PACK(struct DoubleHolderForTransport {
	std::uint64_t mantissa_{0};
	std::int32_t exp_{0};
	FloatPointType type_{FloatPointType::NormalPositive};
});

struct DoubleHolderForUse
{
	std::uint64_t mantissa_{0};
	std::int32_t exp_{0};
	FloatPointType type_{FloatPointType::NormalPositive};
};

inline constexpr std::int32_t double_size{sizeof(DoubleHolderForTransport)};
inline constexpr std::int32_t double_mantissa_53bits{53};
inline constexpr double double_mantissa_53bits_val{0x1FFFFFFFFFFFFF};

using PutInBuf = void (*)(char * to, const char * from);

inline void put_ordered1(char * to, const char * from)
{
	*to = *from;
}

inline void put_ordered2(char * to, const char * from)
{
	memcpy(to, from, 2u);
}

inline void put_ordered4(char * to, const char * from)
{
	memcpy(to, from, 4u);
}

inline void put_ordered8(char * to, const char * from)
{
	memcpy(to, from, 8u);
}

static const PutInBuf put1 = must_reorder ? put_reordered1 : put_ordered1;
static const PutInBuf put2 = must_reorder ? put_reordered2 : put_ordered2;
static const PutInBuf put4 = must_reorder ? put_reordered4 : put_ordered4;
static const PutInBuf put8 = must_reorder ? put_reordered8 : put_ordered8;

inline void get_ordered1(const char * from, char * to)
{
	*to = *from;
}

inline void get_ordered2(const char * from, char * to)
{
	memcpy(to, from, 2u);
}

inline void get_ordered4(const char * from, char * to)
{
	memcpy(to, from, 4u);
}

inline void get_ordered8(const char * from, char * to)
{
	memcpy(to, from, 8u);
}

using GetFromBuf = void (*)(const char * from, char * to);
static const GetFromBuf get1 = must_reorder ? get_reordered1 : get_ordered1;
static const GetFromBuf get2 = must_reorder ? get_reordered2 : get_ordered2;
static const GetFromBuf get4 = must_reorder ? get_reordered4 : get_ordered4;
static const GetFromBuf get8 = must_reorder ? get_reordered8 : get_ordered8;

// Всегда кладу в буффер хранения/передачи данные в LittleEndian

inline void put(char * buf, bool val)
{
	*buf = val ? '1' : '0';
}

inline void put_and_shift(char *& buf, bool val)
{
	put(buf, val);
	++buf;
}

inline void put(char * buf, char val)
{
	*buf = val;
}

inline void put_and_shift(char *& buf, char val)
{
	put(buf, val);
	++buf;
}

inline void put(char * buf, std::int8_t val)
{
	detail::put1(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::int8_t val)
{
	put(buf, val);
	++buf;
}

inline void put(char * buf, std::uint8_t val)
{
	detail::put1(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::uint8_t val)
{
	put(buf, val);
	++buf;
}

inline void put(char * buf, std::int16_t val)
{
	detail::put2(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::int16_t val)
{
	put(buf, val);
	buf += sizeof(std::int16_t);
}

inline void put(char * buf, std::uint16_t val)
{
	detail::put2(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::uint16_t val)
{
	put(buf, val);
	buf += sizeof(std::uint16_t);
}

inline void put(char * buf, std::int32_t val)
{
	detail::put4(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::int32_t val)
{
	put(buf, val);
	buf += sizeof(std::int32_t);
}

inline void put(char * buf, std::uint32_t val)
{
	detail::put4(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::uint32_t val)
{
	put(buf, val);
	buf += sizeof(std::uint32_t);
}

inline void put(char * buf, std::int64_t val)
{
	detail::put8(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::int64_t val)
{
	put(buf, val);
	buf += sizeof(std::int64_t);
}

inline void put(char * buf, std::uint64_t val)
{
	detail::put8(buf, reinterpret_cast<const char *>(&val));
}

inline void put_and_shift(char *& buf, std::uint64_t val)
{
	put(buf, val);
	buf += sizeof(std::uint64_t);
}

inline void put(char * buf, const detail::DoubleHolderForUse & val)
{
	put_and_shift(buf, val.mantissa_);
	put_and_shift(buf, val.exp_);
	put(buf, static_cast<std::uint8_t>(val.type_));
}

inline void put_and_shift(char *& buf, const detail::DoubleHolderForUse & val)
{
	put(buf, val);
	buf += sizeof(detail::DoubleHolderForTransport);
}

inline void put(char * buf, double val)
{
	detail::DoubleHolderForUse holder{};
	do
	{
		if (std::isnan(val))
		{
			holder.type_ = detail::FloatPointType::NaN;
			break;
		}
		if (std::isinf(val))
		{
			holder.type_ = (val < 0.0) ? detail::FloatPointType::InfNegative : detail::FloatPointType::InfPositive;
			break;
		}
		holder.type_ = (val < 0.0) ? detail::FloatPointType::NormalNegative : detail::FloatPointType::NormalPositive;
		double frac = std::frexp(std::fabs(val), &holder.exp_);
		const auto mantissa = std::ldexp(frac, detail::double_mantissa_53bits);
		if (mantissa == HUGE_VAL || mantissa >= detail::double_mantissa_53bits_val)
		{
			holder.type_ = (val < 0.0) ? detail::FloatPointType::InfNegative : detail::FloatPointType::InfPositive;
			break;
		}
		holder.mantissa_ = static_cast<std::uint64_t>(mantissa);
		holder.exp_ -= detail::double_mantissa_53bits;
	}
	while (false);
	put(buf, holder);
}

inline void put_and_shift(char *& buf, double val)
{
	put(buf, val);
	buf += sizeof(detail::DoubleHolderForTransport);
}

inline void put(char * buf, const DsonHeader & val)
{
	put_and_shift(buf, val.key_);
	put_and_shift(buf, val.data_size_);
	put(buf, val.data_type_);
}

inline void put_and_shift(char *& buf, const DsonHeader & val)
{
	put_and_shift(buf, val.key_);
	put_and_shift(buf, val.data_size_);
	put_and_shift(buf, val.data_type_);
}

inline void put(char * buf, const std::string & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
}

inline void put_and_shift(char *& buf, const std::string & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
	buf += size;
}

inline void put(char * buf, const std::vector<std::int8_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
}

inline void put_and_shift(char *& buf, const std::vector<std::int8_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
	buf += size;
}

inline void put(char * buf, const std::vector<std::uint8_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
}

inline void put_and_shift(char *& buf, const std::vector<std::uint8_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	memcpy(buf, val.data(), size);
	buf += size;
}

inline void put(char * buf, const std::vector<std::int16_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::int16_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<std::uint16_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::uint16_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<std::int32_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::int32_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<std::uint32_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::uint32_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<std::int64_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::int64_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<std::uint64_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<std::uint64_t> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const std::vector<double> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put_and_shift(char *& buf, const std::vector<double> & val)
{ // ожидается что размер массива в хидер помещают из холдера
	for (auto it : val)
	{
		put_and_shift(buf, it);
	}
}

inline void put(char * buf, const BufUCharView & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	std::memcpy(buf, val.data(), size);
}

inline void put_and_shift(char *& buf, const BufUCharView & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	std::memcpy(buf, val.data(), size);
	buf += size;
}

inline void put(char * buf, const BufUChar & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	std::memcpy(buf, val.data(), size);
}

inline void put_and_shift(char *& buf, const BufUChar & val)
{ // ожидается что размер массива в хидер помещают из холдера
	const auto size = val.size();
	std::memcpy(buf, val.data(), size);
	buf += size;
}

template <typename T>
inline T align(const char * const val)
{
	T re;
	std::memcpy(&re, val, sizeof(re));
	return re;
}

// Всегда в буффере хранения/передачи данные в LittleEndian
inline void get(const char * buf, bool & val)
{
	val = ('1' == *buf ? true : false);
}

inline void get(const char * buf, char & val)
{
	val = *buf;
}

inline void get(const char * buf, std::int8_t & val)
{
	detail::get1(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::uint8_t & val)
{
	detail::get1(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::int16_t & val)
{
	detail::get2(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::uint16_t & val)
{
	detail::get2(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::int32_t & val)
{
	detail::get4(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::uint32_t & val)
{
	detail::get4(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::int64_t & val)
{
	detail::get8(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, std::uint64_t & val)
{
	detail::get8(buf, reinterpret_cast<char *>(&val));
}

inline void get(const char * buf, detail::DoubleHolderForUse & val)
{
	get(buf, val.mantissa_);
	buf += sizeof(val.mantissa_);
	get(buf, val.exp_);
	buf += sizeof(val.exp_);
	std::uint8_t type;
	get(buf, type);
	val.type_ = static_cast<FloatPointType>(type);
}

inline void get(const char * buf, double & val)
{
	detail::DoubleHolderForUse holder;
	get(buf, holder);
	switch (holder.type_)
	{
	case detail::FloatPointType::NaN:
		val = std::numeric_limits<double>::quiet_NaN();
		return;
	case detail::FloatPointType::InfNegative:
		val = -std::numeric_limits<double>::infinity();
		return;
	case detail::FloatPointType::InfPositive:
		val = std::numeric_limits<double>::infinity();
		return;
	default:
		break;
	}
	val = std::ldexp(static_cast<double>(holder.mantissa_), holder.exp_);
	if (holder.type_ == detail::FloatPointType::NormalNegative)
	{
		val = -val;
	}
}

inline void get(const char * buf, DsonHeader & val)
{
	get(buf, val.key_);
	buf += sizeof(val.key_);
	get(buf, val.data_size_);
	buf += sizeof(val.data_size_);
	get(buf, val.data_type_);
}

inline Key get_key(const char * buf)
{
	Key re;
	get(buf, re);
	return re;
}

inline void put_key(char * buf, Key key)
{
	put(buf, key);
}

inline std::int32_t get_data_size(const char * buf)
{
	std::int32_t re;
	get(buf + sizeof(Key), re);
	return re;
}

inline void put_data_size(char * buf, std::int32_t data_size)
{
	buf += sizeof(Key);
	put(buf, data_size);
}

inline std::int32_t get_data_type(const char * buf)
{
	std::int32_t re;
	get(buf + sizeof(Key) + sizeof(std::int32_t), re);
	return re;
}

inline void put_data_type(char * buf, std::int32_t data_type)
{
	buf += sizeof(Key) + sizeof(std::int32_t);
	put(buf, data_type);
}

inline void get_and_shift(const char *& buf, bool & val)
{
	get(buf, val);
	++buf;
}

inline void get_and_shift(const char *& buf, char & val)
{
	val = *buf;
	++buf;
}

inline void get_and_shift(const char *& buf, std::int8_t & val)
{
	get(buf, val);
	++buf;
}

inline void get_and_shift(const char *& buf, std::uint8_t & val)
{
	get(buf, val);
	++buf;
}

inline void get_and_shift(const char *& buf, std::int16_t & val)
{
	get(buf, val);
	buf += 2u;
}

inline void get_and_shift(const char *& buf, std::uint16_t & val)
{
	get(buf, val);
	buf += 2u;
}

inline void get_and_shift(const char *& buf, std::int32_t & val)
{
	get(buf, val);
	buf += 4u;
}

inline void get_and_shift(const char *& buf, std::uint32_t & val)
{
	get(buf, val);
	buf += 4u;
}

inline void get_and_shift(const char *& buf, std::int64_t & val)
{
	get(buf, val);
	buf += 8u;
}

inline void get_and_shift(const char *& buf, std::uint64_t & val)
{
	get(buf, val);
	buf += 8u;
}

inline void get_and_shift(const char *& buf, detail::DoubleHolderForUse & val)
{
	get(buf, val);
	buf += sizeof(detail::DoubleHolderForTransport);
}

inline void get_and_shift(const char *& buf, double & val)
{
	get(buf, val);
	buf += sizeof(detail::DoubleHolderForTransport);
}

inline void get_and_shift(const char *& buf, DsonHeader & val)
{
	get(buf, val);
	buf += sizeof(detail::Header);
}

inline void get(const char * buf, std::int32_t data_size, std::string & val)
{
	val = std::string{buf, static_cast<std::size_t>(data_size)};
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::int8_t> & val)
{
	val = std::vector<std::int8_t>{
		reinterpret_cast<const std::int8_t *>(buf),
		reinterpret_cast<const std::int8_t *>(buf + data_size)};
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::uint8_t> & val)
{
	val = std::vector<std::uint8_t>{
		reinterpret_cast<const std::uint8_t *>(buf),
		reinterpret_cast<const std::uint8_t *>(buf + data_size)};
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::int16_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::int16_t));
	val.reserve(cnt);
	std::int16_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::uint16_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::uint16_t));
	val.reserve(cnt);
	std::uint16_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::int32_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::int32_t));
	val.reserve(cnt);
	std::int32_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::uint32_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::uint32_t));
	val.reserve(cnt);
	std::uint32_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::int64_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::int64_t));
	val.reserve(cnt);
	std::int64_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<std::uint64_t> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / sizeof(std::uint64_t));
	val.reserve(cnt);
	std::uint64_t tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, std::vector<double> & val)
{
	val.clear();
	const auto cnt = static_cast<std::size_t>(data_size / detail::double_size);
	val.reserve(cnt);
	double tmp;
	for (std::size_t i = 0u; i < cnt; ++i)
	{
		get_and_shift(buf, tmp);
		val.emplace_back(tmp);
	}
}

inline void get(const char * buf, std::int32_t data_size, BufUCharView & val)
{
	val = BufUCharView{reinterpret_cast<const BufUCharView::value_type *>(buf), data_size};
}

inline void get(const char * buf, std::int32_t data_size, BufUChar & val)
{
	val = BufUChar{reinterpret_cast<const BufUChar::value_type *>(buf), data_size};
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::string & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::int8_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::uint8_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::int16_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::uint16_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::int32_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::uint32_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::int64_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<std::uint64_t> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, std::vector<double> & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, BufUCharView & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

inline void get_and_shift(const char *& buf, std::int32_t data_size, BufUChar & val)
{
	get(buf, data_size, val);
	buf += data_size;
}

} // namespace detail
} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DETAIL_HEADER_H
