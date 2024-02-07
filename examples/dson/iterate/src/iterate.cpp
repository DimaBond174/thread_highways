#include <thread_highways/dson/include_all.h>
#include <thread_highways/dson/dson_edit_controller_printer.h>

#include <iostream>
#include <string>

enum class Key : std::int32_t
{
	Name,
	X,
	Y,
	Root,
	Child1,
	Child2,
	Child3,
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

void print_dson_edit_controller()
{
	//    int i = 1;
	//    try {
	//        HI_ASSERT(i > 1000);
	//    } catch (hi::Exception e) {
	//        std::cerr << e << std::endl;
	//    }
	std::cout << "print_dson_edit_controller() ==========\n";
	{
		hi::dson::NewDson dson;
		dson.emplace(Key::Name, std::string{"Child1 caption"});
		// dson.emplace(Key::X, double{-11.11111});
		dson.emplace(Key::X, std::string{"-11.11111"});
		// dson.emplace(Key::Y, std::int64_t{11111});
		dson.emplace(Key::Y, std::string{"22222"});
		dson.set_key_cast(Key::Child3);

		hi::dson::DsonEditController dson_on_disk{"any_path.bin"};
		dson_on_disk.set_key_cast(Key::Root);
		dson_on_disk.emplace(Key::Name, std::string{"Root caption"});
		dson_on_disk.emplace(std::move(dson));

		std::cout << "print before save ==========\n";
		std::cout << dson_on_disk << std::endl;
		dson_on_disk.save();
		std::cout << "print after save ==========\n";
		std::cout << dson_on_disk << std::endl;
	}

	{
		std::cout << "Open №1:\n +++++++++++++++++++++";
		hi::dson::DsonEditController dson_on_disk;
		dson_on_disk.open("any_path.bin");
		const auto size_before1 = dson_on_disk.all_size();
		const auto size_on_disk1 = dson_on_disk.current_file_size_before_save();
		HI_ASSERT(size_before1 == size_on_disk1);

		std::cout << "print after open ==========\n";
		std::cout << dson_on_disk << std::endl;

		const auto size_before2 = dson_on_disk.all_size();
		const auto size_on_disk2 = dson_on_disk.current_file_size_before_save();
		HI_ASSERT(size_before2 == size_on_disk2);
		HI_ASSERT(size_on_disk1 == size_on_disk2);
		HI_ASSERT(size_before1 == size_before2);

		dson_on_disk.open_folder(1);
		dson_on_disk.emplace(Key::X, std::string{"WWWWW"});
		const auto size_after = dson_on_disk.all_size();
		HI_ASSERT(size_before1 > size_after);

		std::cout << "print before save ==========\n";
		std::cout << dson_on_disk << std::endl;
		dson_on_disk.save();
		std::cout << "print after save ==========\n";
		std::cout << dson_on_disk << std::endl;
	}

	{
		std::cout << "Open №2:\n";
		hi::dson::DsonEditController dson_on_disk;
		dson_on_disk.open("any_path.bin");
		std::cout << dson_on_disk << std::endl;
	}
}

void default_editor_save_as()
{ // По умолчанию создаётся временный файл который нужно будет сохранить как постоянный файл
	std::cout << "default_editor_save_as()===============================\n";
	{
		hi::dson::DsonEditController dson_controller;
		dson_controller.create_default("./");

		dson_controller.set_key_cast(Key::Root);
		dson_controller.emplace(Key::Name, std::string{"Root caption"});
		dson_controller.emplace(Key::Child3, std::string{"Key::Child3"});

		auto cnt = dson_controller.items_on_level();
		HI_ASSERT(cnt == 2u);

        std::cout << dson_controller << std::endl;

		dson_controller.save_as("test.dson");
	}
	{
		std::cout << "Open №2:\n";
		hi::dson::DsonEditController dson_on_disk;
		dson_on_disk.open("test.dson");
		std::cout << dson_on_disk << std::endl;
	}
}

void print_dson_file(std::string path)
{
    std::cout << "===============================\n"
     << path << "\n===============================\n";
        hi::dson::DsonEditController dson_on_disk;
        dson_on_disk.open(path);
        std::cout << dson_on_disk << std::endl;
}

int main(int /* argc */, char ** /* argv */)
{
    print_dson_edit_controller();
	default_editor_save_as();
    //print_dson_file("/home/dik/workspace/tmp/dson123.dson");
	std::cout << "Tests finished" << std::endl;
	return 0;
}
