#include <thread_highways/dson/include_all.h>

#include <iostream>

enum class Key : std::int32_t
{
	k_uint32 = 1,
	k_int64 = 2,
	k_double = 3,
	k_string = 4,
};

template <typename ValueType>
void try_get(hi::dson::NewDson * dson, Key key)
{
	ValueType val;
	if (dson->get(key, val))
	{
		std::cout << "dson.get(" << static_cast<int32_t>(key) << ") == " << val << std::endl;
	}
	else
	{
		std::cout << "Failed dson.get(" << static_cast<int32_t>(key) << ") of ValueType==" << typeid(ValueType).name()
				  << std::endl;
	}
}

void save_bin(const std::string & file_name, const char * data, uint32_t size)
{
	std::ofstream fout(file_name, std::ios::out | std::ios::binary);
	fout.write(data, size);
	fout.close();
	return;
}

void simple_uploader()
{
	hi::dson::NewDson dson;
	std::int32_t i = 1;
	dson.set_key(i);
	dson.emplace(Key::k_uint32, static_cast<std::uint32_t>(i * 100));
	dson.emplace(Key::k_int64, static_cast<std::int64_t>(i * 1000));
	dson.emplace(Key::k_double, static_cast<double>(i * 100.1));
	dson.emplace(Key::k_string, std::to_string(i * 100.1));

	std::cout << "================\n";
	std::cout << "dson[" << dson.obj_key() << "].size=" << dson.all_size() << std::endl;
	try_get<std::uint32_t>(&dson, Key::k_uint32);
	try_get<std::int64_t>(&dson, Key::k_int64);
	try_get<double>(&dson, Key::k_double);
	try_get<std::string>(&dson, Key::k_string);
	std::cout << "-------------------------------\n";

	hi::dson::DsonFromBuff dson_from_fuff;
	HI_ASSERT(dson_from_fuff.upload(dson) > 0);

	std::cout << "================\n";
	std::cout << "dson_from_fuff[" << dson_from_fuff.obj_key() << "].size=" << dson_from_fuff.all_size() << std::endl;
	try_get<std::uint32_t>(&dson_from_fuff, Key::k_uint32);
	try_get<std::int64_t>(&dson_from_fuff, Key::k_int64);
	try_get<double>(&dson_from_fuff, Key::k_double);
	try_get<std::string>(&dson_from_fuff, Key::k_string);
	std::cout << "-------------------------------\n";

	hi::dson::DsonFromBuff dson_from_fuff2;
	HI_ASSERT(dson_from_fuff2.upload(dson_from_fuff) > 0);

	std::cout << "================\n";
	std::cout << "dson_from_fuff2[" << dson_from_fuff2.obj_key() << "].size=" << dson_from_fuff2.all_size()
			  << std::endl;
	try_get<std::uint32_t>(&dson_from_fuff2, Key::k_uint32);
	try_get<std::int64_t>(&dson_from_fuff2, Key::k_int64);
	try_get<double>(&dson_from_fuff2, Key::k_double);
	try_get<std::string>(&dson_from_fuff2, Key::k_string);
	std::cout << "-------------------------------\n";
}

int main(int /* argc */, char ** /* argv */)
{
	simple_uploader();
	std::cout << "Tests finished" << std::endl;
	return 0;
}
