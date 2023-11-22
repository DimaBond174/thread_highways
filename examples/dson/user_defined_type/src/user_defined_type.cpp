#include <thread_highways/dson/include_all.h>

#include <iostream>

struct MyStruct : public hi::dson::Serializable
{
	MyStruct() = default;
	MyStruct(std::string _name, std::uint16_t _age, bool _male)
		: name{std::move(_name)}
		, age{_age}
		, male{_male}
	{
	}

	std::string name;
	std::uint16_t age{0};
	bool male{false};

	//  hi::dson::Serializable
	std::string serialize() const override
	{
		const auto all_size = sizeof(std::int32_t) /* для name.size() */ + name.size() + sizeof(age) + sizeof(male);
		std::string re(all_size, 0);
		char * buf = re.data();
		hi::dson::detail::put_and_shift(buf, static_cast<std::int32_t>(name.size()));
		hi::dson::detail::put_and_shift(buf, name);
		hi::dson::detail::put_and_shift(buf, age);
		hi::dson::detail::put_and_shift(buf, male);
		auto diff = (buf - re.data() - all_size);
		assert(0 == diff);
		return re;
	}

	bool deserialize(const char * buf, std::int32_t size) override
	{
		auto end = buf + size;
		std::int32_t name_size;
		hi::dson::detail::get_and_shift(buf, name_size);
		hi::dson::detail::get_and_shift(buf, name_size, name);
		hi::dson::detail::get_and_shift(buf, age);
		hi::dson::detail::get_and_shift(buf, male);
		return end == buf;
	}
};

void save_and_load_simple()
{
	MyStruct my_struct_before{"Dima", 45, true};

	{
		hi::dson::NewDson dson;
		dson.emplace_serializable(1, my_struct_before);

		std::ofstream output("dson.bin", std::ios::binary);

		hi::dson::UploaderToStream uploader;
		auto res = uploader.upload(output, dson);
		if (res < 0)
		{
			std::cout << "Fail uploader.upload with error: " << res << std::endl;
		}
	}

	std::ifstream input("dson.bin", std::ios::binary);

	auto dson = hi::dson::DownloaderFromStream::load(input);
	MyStruct my_struct_after;
	dson->get_serializable(1, my_struct_after);

	assert(
		my_struct_before.name == my_struct_after.name && my_struct_before.age == my_struct_after.age
		&& my_struct_before.male == my_struct_after.male);

	std::cout << "Loaded:" << std::endl
			  << "MyStruct {" << std::endl
			  << "\t name = " << my_struct_after.name << std::endl
			  << "\t age = " << my_struct_after.age << std::endl
			  << "\t male = " << my_struct_after.male << std::endl
			  << "}" << std::endl;
}

int main(int /* argc */, char ** /* argv */)
{
	save_and_load_simple();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
