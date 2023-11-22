#include <thread_highways/dson/include_all.h>
#include <thread_highways/dson/dson_from_file_lifetime_controller.h>

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class Key : std::int32_t
{
	k_uint32 = 1,
	k_int64 = 2,
	k_double = 3,
	k_string = 4,
};

template <typename ValueType>
void try_get(std::unique_ptr<hi::dson::NewDson> & dson, Key key)
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
	auto dson = std::make_unique<hi::dson::NewDson>();
	std::int32_t i = 1;
	dson->set_key(i);
	dson->emplace(Key::k_uint32, static_cast<std::uint32_t>(i * 100));
	dson->emplace(Key::k_int64, static_cast<std::int64_t>(i * 1000));
	dson->emplace(Key::k_double, static_cast<double>(i * 100.1));
	dson->emplace(Key::k_string, std::to_string(i * 100.1));

	std::cout << "================\n";
	std::cout << "sended dson[" << dson->obj_key() << "].size=" << dson->all_size() << std::endl;
	try_get<std::uint32_t>(dson, Key::k_uint32);
	try_get<std::int64_t>(dson, Key::k_int64);
	try_get<double>(dson, Key::k_double);
	try_get<std::string>(dson, Key::k_string);
	std::cout << "-------------------------------\n";

	std::vector<char> sended_data;
	std::int32_t all_size = dson->all_size();
	sended_data.resize(static_cast<size_t>(all_size));

	hi::dson::UploaderToBuffHolder uploader;
	uploader.set(std::move(dson));
	hi::result_t res = hi::okInProcess;
	char * cur_buf = sended_data.data();
	while (hi::okInProcess == res)
	{
		HI_ASSERT(all_size != 0);
		res = uploader.upload_step(cur_buf, all_size);
	}
	HI_ASSERT(all_size == 0);
}

void simple_uploader_step4()
{
	std::cout << " simple_uploader_step4 ==========================" << std::endl;
	auto dson = std::make_unique<hi::dson::NewDson>();
	std::int32_t i = 1;
	dson->set_key(i);
	dson->emplace(Key::k_uint32, static_cast<std::uint32_t>(i * 100));
	dson->emplace(Key::k_int64, static_cast<std::int64_t>(i * 1000));
	dson->emplace(Key::k_double, static_cast<double>(i * 100.1));
	dson->emplace(Key::k_string, std::to_string(i * 100.1));

	std::cout << "================\n";
	std::cout << "sended dson[" << dson->obj_key() << "].size=" << dson->all_size() << std::endl;
	try_get<std::uint32_t>(dson, Key::k_uint32);
	try_get<std::int64_t>(dson, Key::k_int64);
	try_get<double>(dson, Key::k_double);
	try_get<std::string>(dson, Key::k_string);
	std::cout << "-------------------------------\n";

	std::vector<char> sended_data;
	std::int32_t all_size = dson->all_size();
	sended_data.resize(static_cast<size_t>(all_size));

	hi::dson::UploaderToBuffHolder uploader;
	uploader.set(std::move(dson));
	hi::result_t res = hi::okInProcess;
	char * cur_buf = sended_data.data();
	while (hi::okInProcess == res)
	{
		HI_ASSERT(all_size != 0);
		std::int32_t unused_size = 4;
		res = uploader.upload_step(cur_buf, unused_size);
		std::int32_t uploaded_size = 4 - unused_size;
		std::cout << " uploaded_size=" << uploaded_size << std::endl;
		all_size -= (uploaded_size);
	}
	HI_ASSERT(all_size == 0);
	save_bin("simple_uploader_step4" + std::to_string(i), sended_data.data(), sended_data.size());
}

void shared_buf_test(std::shared_ptr<hi::dson::DsonFromFileController> controller)
{
	std::cout << " shared_buf_test ==========================" << std::endl;
	// shared_buf[0] = 0 == можно отправлять, 1 == можно принимать (и наоборот: если можно отправлять, то нельзя
	// принимать)
	std::vector<char> shared_buf(5, 0);
	// можно обойтись без condition_variable - atomic_fence синхронизировать потоки на просмотр shared_buf[0],
	// но тогда будет необходимо отправлять потоки в сон == возможные простои на время сна
	// (это может быть оправдано при IPC если накладные расходы на boost выше)
	// TODO сравнительнение скорости IPC через boost IPC мьютексы vs sleep&atomic_fence
	const hi::dson::Key test_size{100};

	std::mutex buf_mutex;
	std::condition_variable buf_cv;

	// общий флаг для потоков о том что пора заканчивать работу
	std::atomic<bool> go_shutdown{false};
	std::atomic<bool> receiver_shutdown{false};

	const auto buf_size = static_cast<std::int32_t>(shared_buf.size() - 1);
	const auto add_buf = [&](std::vector<char> & to, std::int32_t size)
	{
		++size; // i from 1
		for (std::int32_t i = 1; i < size; ++i)
		{
			to.push_back(shared_buf[i]);
		}
	};

	auto sender = [&]
	{
		hi::dson::UploaderToBuffHolder uploader;
		std::unique_lock lk(buf_mutex);
		for (hi::dson::Key i = 1; i < test_size; ++i)
		{
			auto dson = std::make_unique<hi::dson::NewDson>();
			dson->set_key(i);
			dson->emplace(Key::k_uint32, static_cast<std::uint32_t>(i * 100));
			dson->emplace(Key::k_int64, static_cast<std::int64_t>(i * 1000));
			dson->emplace(Key::k_double, static_cast<double>(i * 100.1));
			dson->emplace(Key::k_string, std::to_string(i * 100.1));
			{
				std::cout << "================\n";
				std::cout << "sended dson[" << dson->obj_key() << "].size=" << dson->all_size() << std::endl;
				try_get<std::uint32_t>(dson, Key::k_uint32);
				try_get<std::int64_t>(dson, Key::k_int64);
				try_get<double>(dson, Key::k_double);
				try_get<std::string>(dson, Key::k_string);
				std::cout << "-------------------------------\n";
			}
			std::vector<char> sended_data;
			std::int32_t all_size = dson->all_size();
			sended_data.reserve(static_cast<size_t>(all_size));

			uploader.set(std::move(dson));

			const auto upload = [&]() -> hi::result_t
			{
				hi::result_t res = hi::okInProcess;
				if (shared_buf[0])
					return res;
				char * cur_buf = shared_buf.data() + 1;

				std::int32_t cur_buf_size = buf_size;
				while (hi::okInProcess == res && cur_buf_size)
				{ // тут важно: каждый шаг может наполнять буффер не полностью,
					// а получатель ожидает что если это ещё не конец, то буффер заполнен полностью
					// поэтому цикл
					res = uploader.upload_step(cur_buf, cur_buf_size);
				}

				HI_ASSERT(res > 0);
				if (buf_size != cur_buf_size)
				{
					shared_buf[0] = 1;
				}
				else
				{
					assert(false);
				}
				auto uploaded_size = buf_size - cur_buf_size;
				std::cout << "uploaded_size=" << uploaded_size << std::endl;
				add_buf(sended_data, uploaded_size);
				buf_cv.notify_one();
				return res;
			};

			buf_cv.wait(
				lk,
				[&]() -> bool
				{
					if (go_shutdown)
						return true;
					if (hi::okInProcess == upload())
						return false;
					return true;
				});
			HI_ASSERT(sended_data.size() == static_cast<size_t>(all_size));
			save_bin("sended_data" + std::to_string(i), sended_data.data(), sended_data.size());

		} // for
	};

	auto receiver = [&]
	{
		hi::dson::DownloaderFromSharedBuf downloader;
		if (controller)
		{
			downloader.setup_dson_from_file(controller);
		}
		std::unique_lock lk(buf_mutex);
		buf_cv.wait(
			lk,
			[&]() -> bool
			{
				if (go_shutdown)
					return true;
				if (!shared_buf[0])
					return false;
				auto res = downloader.load(shared_buf.data() + 1, static_cast<std::int32_t>(shared_buf.size() - 1));
				assert(res > 0);
				if (hi::okReady == res)
				{
					auto dson = downloader.get_result();
					std::cout << "received dson[" << dson->obj_key() << "]\n";
					try_get<std::uint32_t>(dson, Key::k_uint32);
					try_get<std::int64_t>(dson, Key::k_int64);
					try_get<double>(dson, Key::k_double);
					try_get<std::string>(dson, Key::k_string);
				}
				shared_buf[0] = 0;
				buf_cv.notify_one();
				return false;
			});
		receiver_shutdown = true;
	};

	std::thread th1{std::move(sender)};
	std::thread th2{std::move(receiver)};
	buf_cv.notify_all();
	th1.join();
	go_shutdown = true;
	while (receiver_shutdown == false)
	{
		buf_cv.notify_all();
	}
	th2.join();
}

int main(int /* argc */, char ** /* argv */)
{
	// simple_uploader();
	// simple_uploader_step4();
	shared_buf_test({});
	// shared_buf_test(std::make_shared<hi::dson::DsonFromFileLifetimeController>(1, "./"));
	std::cout << "Tests finished" << std::endl;
	return 0;
}
