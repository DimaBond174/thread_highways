/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_PRINTER_H
#define THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_PRINTER_H

#include <thread_highways/dson/dson_edit_controller.h>

#include <ostream>

namespace hi
{
namespace dson
{
namespace detail
{

void print_string_on_level(const std::string & str, std::ostream & stream, std::uint32_t level)
{
	for (uint32_t i = 0; i < level; ++i)
	{
		stream << ' ';
	}
	stream << str << std::endl;
}

template <typename T>
void print_number_casted(IObjView * obj, std::ostream & stream, std::uint32_t level)
{
	T val;
	if (cast<T>(val, obj))
	{
		print_string_on_level("\"data\":" + std::to_string(val), stream, level);
	}
	else
	{
		print_string_on_level("\"data\":\"cast error\"", stream, level);
	}
}

template <typename T>
void print_vector_casted(IObjView * obj, std::ostream & stream, std::uint32_t level)
{
	T val;
	if (cast<T>(val, obj))
	{
		std::string str{"\"data\":["};
		for (auto it : val)
		{
			str.append(std::to_string(it));
			str.push_back(',');
		}
		if (str[str.length() - 1] == ',')
		{
			str[str.length() - 1] = ']';
		}
		print_string_on_level(str, stream, level);
	}
	else
	{
		print_string_on_level("\"data\":\"cast error\"", stream, level);
	}
}

void print_string_casted(IObjView * obj, std::ostream & stream, std::uint32_t level)
{
	std::string val;
	if (cast<std::string>(val, obj))
	{
		print_string_on_level("\"data\":\"" + val + "\"", stream, level);
	}
	else
	{
		print_string_on_level("\"data\":\"cast error\"", stream, level);
	}
}

void print_object(IObjView * obj, std::ostream & stream, std::uint32_t level)
{
	switch (obj->data_type())
	{
	case detail::types_map<bool>::value:
		print_number_casted<bool>(obj, stream, level);
		break;
	case detail::types_map<std::int8_t>::value:
		print_number_casted<std::int8_t>(obj, stream, level);
		break;
	case detail::types_map<std::uint8_t>::value:
		print_number_casted<std::uint8_t>(obj, stream, level);
		break;
	case detail::types_map<std::int16_t>::value:
		print_number_casted<std::int16_t>(obj, stream, level);
		break;
	case detail::types_map<std::uint16_t>::value:
		print_number_casted<std::uint16_t>(obj, stream, level);
		break;
	case detail::types_map<std::int32_t>::value:
		print_number_casted<std::int32_t>(obj, stream, level);
		break;
	case detail::types_map<std::uint32_t>::value:
		print_number_casted<std::uint32_t>(obj, stream, level);
		break;
	case detail::types_map<std::int64_t>::value:
		print_number_casted<std::int64_t>(obj, stream, level);
		break;
	case detail::types_map<std::uint64_t>::value:
		print_number_casted<std::uint64_t>(obj, stream, level);
		break;
	case detail::types_map<double>::value:
		print_number_casted<double>(obj, stream, level);
		break;
	case detail::types_map<std::string>::value:
		print_string_casted(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::int8_t>>::value:
		print_vector_casted<std::vector<std::int8_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::uint8_t>>::value:
		print_vector_casted<std::vector<std::uint8_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::int16_t>>::value:
		print_vector_casted<std::vector<std::int16_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::uint16_t>>::value:
		print_vector_casted<std::vector<std::uint16_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::int32_t>>::value:
		print_vector_casted<std::vector<std::int32_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::uint32_t>>::value:
		print_vector_casted<std::vector<std::uint32_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::int64_t>>::value:
		print_vector_casted<std::vector<std::int64_t>>(obj, stream, level);
		break;
	case detail::types_map<std::vector<std::uint64_t>>::value:
		print_vector_casted<std::vector<std::uint64_t>>(obj, stream, level);
		break;
	case detail::types_map<BufUCharView>::value:
		print_string_on_level("\"data\":\"BufUCharView\"", stream, level);
		break;
	case detail::types_map<BufUChar>::value:
		print_string_on_level("\"data\":\"BufUChar\"", stream, level);
		break;
	default:
		print_string_on_level("\"data\":\"type error\"", stream, level);
		break;
	}
}

void print_dson(std::ostream & stream, DsonEditController & dson, std::uint32_t level)
{
	print_string_on_level("\"data\": [", stream, level);
	const auto size = dson.items_on_level();
	for (std::size_t i = 0; i < size; ++i)
	{
		auto obj = dson[i]->self();
		print_string_on_level("{\"key\":" + std::to_string(obj->obj_key()) + ",", stream, level);
		if (obj->data_type() == detail::types_map<detail::DsonContainer>::value)
		{
			dson.open_folder(i);
			print_dson(stream, dson, level + 2u);
			std::size_t index;
			dson.close_folder(index);
			HI_ASSERT(i == index);
		}
		else
		{
			print_object(obj, stream, level + 2u);
		}
		detail::print_string_on_level(i < size - 1 ? "}," : "}", stream, level);
	}
	print_string_on_level("]", stream, level);
}

} // namespace detail

std::ostream & operator<<(std::ostream & stream, DsonEditController & dson)
{
	// Встаём в исходное
	std::size_t out_index;
	while (ok == dson.close_folder(out_index))
	{
	}

	// Обходим каталоги
	detail::print_string_on_level("{\"key\":" + std::to_string(dson.obj_key()) + ",", stream, 0u);
	detail::print_dson(stream, dson, 2u);
	detail::print_string_on_level("}", stream, 0u);

	return stream;
}

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DSON_EDIT_CONTROLLER_PRINTER_H
