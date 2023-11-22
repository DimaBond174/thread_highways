#include <thread_highways/dson/include_all.h>

#include <iostream>
#include <string>

enum class Key : std::int32_t
{
	x,
	y,
	z
};

template <typename ValueType>
void try_get(hi::dson::NewDson & dson, Key key)
{
	ValueType val;
	if (dson.get(key, val))
	{
		std::cout << "dson.get(" << static_cast<int32_t>(key) << ") == " << val << std::endl;
	}
	else
	{
		std::cout << "Failed dson.get(" << static_cast<int32_t>(key) << ") of ValueType==" << typeid(ValueType).name()
				  << std::endl;
	}
}

void try_get_without_copy(hi::dson::NewDson & dson, Key key)
{
	hi::BufUCharView val;
	if (dson.get(key, val))
	{
		std::cout << "try_get_without_copy dson.get(" << static_cast<int32_t>(key) << ") == "
				  << std::string{reinterpret_cast<const char *>(val.data()), static_cast<std::size_t>(val.size())}
				  << std::endl;
	}
	else
	{
		std::cout << "Failed try_get_without_copy dson.get(" << static_cast<int32_t>(key)
				  << ") of ValueType==VectorNoCopyAdapter" << std::endl;
	}
}

void store_numbers()
{
	std::cout << "store_numbers() ==========\n";
	hi::dson::NewDson dson;
	dson.emplace(Key::z, std::uint32_t{12345});
	dson.emplace(Key::x, std::int32_t{1});
	dson.emplace(Key::y, double{-2.2});
	dson.emplace(Key::z, std::uint64_t{3});

	std::cout << "Key::z:\n";
	try_get<std::uint32_t>(dson, Key::z);
	try_get<std::uint64_t>(dson, Key::z);
	dson.find(
		static_cast<hi::dson::Key>(Key::z),
		[](hi::dson::IObjView * view)
		{
			auto casted = dynamic_cast<hi::dson::DsonObj<std::uint64_t> *>(view);
			std::uint64_t number;
			casted->get(number);
			assert(number == std::uint64_t{3});
		});

	std::cout << "Key::x:\n";
	try_get<std::int32_t>(dson, Key::x);

	std::cout << "Key::y:\n";
	try_get<double>(dson, Key::y);
}

void store_strings()
{
	std::cout << "store_strings() ==========\n";
	hi::dson::NewDson dson;
	dson.emplace(Key::x, std::string{"Mother washed the frame"});
	dson.emplace(Key::y, std::string{"Father eats banana"});

	std::cout << "Key::x:\n";
	try_get<std::string>(dson, Key::x);
	try_get<std::string_view>(dson, Key::x);

	std::cout << "Key::y:\n";
	try_get<std::int32_t>(dson, Key::y);
	try_get<std::string>(dson, Key::y);

	try_get_without_copy(dson, Key::x);
	try_get_without_copy(dson, Key::y);
}

void store_double(double val)
{
	std::cout << "store_double() ==========\n";
	hi::dson::NewDson dson;
	dson.emplace(0, val);
	double res{};
	dson.get(0, res);
	std::cout << val << "==" << res << std::endl;
}

void dson_from_file()
{
	std::cout << "dson_from_file() ==========\n";
	{
		hi::dson::NewDson dson;
		dson.emplace(Key::x, std::string{"Very big data"});
		dson.emplace(Key::y, double{-12.345678});
		dson.emplace(Key::z, std::int64_t{98765});
		dson.set_key(12345);

		hi::dson::DsonFromFile dson_on_disk;
		assert(dson_on_disk.create("any_path.bin") > 0);
		assert(dson_on_disk.upload(dson) == hi::okReady);
		try_get<std::string>(dson_on_disk, Key::x);
		dson_on_disk.save();
		try_get<std::string>(dson_on_disk, Key::x);
	}

	{
		std::cout << "Open №1:\n";
		hi::dson::DsonFromFile dson_on_disk;
		dson_on_disk.open("any_path.bin");
		std::cout << "Key::x:\n";
		try_get<std::string>(dson_on_disk, Key::x);

		std::cout << "Key::y:\n";
		try_get<double>(dson_on_disk, Key::y);

		std::cout << "Key::z:\n";
		try_get<std::int64_t>(dson_on_disk, Key::z);

		dson_on_disk.emplace(Key::x, std::string{"WWWWW"});
		dson_on_disk.save();
	}

	{
		std::cout << "Open №2:\n";
		hi::dson::DsonFromFile dson_on_disk;
		dson_on_disk.open("any_path.bin");
		std::cout << "Key::x:\n";
		try_get<std::string>(dson_on_disk, Key::x);

		std::cout << "Key::y:\n";
		try_get<double>(dson_on_disk, Key::y);

		std::cout << "Key::z:\n";
		try_get<std::int64_t>(dson_on_disk, Key::z);
	}
}

int main(int /* argc */, char ** /* argv */)
{
	store_numbers();
	store_strings();
	store_double(12345.54321);
	store_double(-12345.54321);
	dson_from_file();
	std::cout << "Tests finished" << std::endl;
	return 0;
}
