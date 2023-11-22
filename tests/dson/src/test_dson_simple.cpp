/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/dson/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{

enum class Key : std::int32_t
{
	x1,
	x2,
	x3,
	x4,
	x5,
	x6,
	x7,
	x8,
};

constexpr std::int32_t i32{-2147483647};
constexpr std::uint32_t ui32{4294967295u};
constexpr std::int64_t i64{-9223372036854775807ll};
constexpr std::uint64_t ui64{18446744073709551615ull};
constexpr double d1{-18446744.07371};
constexpr double d2{18446744.07371};
const std::string str1{"Mother washed the frame. Мама мыла раму."};
const std::string str2{"Father eats banana. Папа ел банан"};
const std::vector<double> vec1{d1, d2, 12345.6789};
const std::string file_path{"any_path.bin"};

using dson_types = ::testing::Types<hi::dson::NewDson, hi::dson::DsonFromBuff, hi::dson::DsonFromFile>;

template <class T>
struct TestDsonSimple : public ::testing::Test
{
};
TYPED_TEST_SUITE(TestDsonSimple, dson_types);

} // namespace

TYPED_TEST(TestDsonSimple, EmplaceAndGet)
{
	TypeParam dson;
	dson.emplace(Key::x1, i32);
	dson.emplace(Key::x2, ui32);
	dson.emplace(Key::x3, i64);
	dson.emplace(Key::x4, ui64);
	dson.emplace(Key::x5, d1);
	dson.emplace(Key::x6, str1);

	std::int32_t res_i32;
	EXPECT_TRUE(dson.get(Key::x1, res_i32));
	EXPECT_EQ(res_i32, i32);

	std::uint32_t res_ui32;
	EXPECT_TRUE(dson.get(Key::x2, res_ui32));
	EXPECT_EQ(res_ui32, ui32);

	std::int64_t res_i64;
	EXPECT_TRUE(dson.get(Key::x3, res_i64));
	EXPECT_EQ(res_i64, i64);

	std::uint64_t res_ui64;
	EXPECT_TRUE(dson.get(Key::x4, res_ui64));
	EXPECT_EQ(res_ui64, ui64);

	double res_d;
	EXPECT_TRUE(dson.get(Key::x5, res_d));
	EXPECT_EQ(res_d, d1);

	std::string str;
	EXPECT_TRUE(dson.get(Key::x6, str));
	EXPECT_EQ(str, str1);

	dson.emplace(Key::x1, d2);
	dson.emplace(Key::x7, str2);

	EXPECT_TRUE(dson.get(Key::x1, res_d));
	EXPECT_EQ(res_d, d2);

	EXPECT_TRUE(dson.get(Key::x5, res_d));
	EXPECT_EQ(res_d, d1);

	EXPECT_TRUE(dson.get(Key::x7, str));
	EXPECT_EQ(str, str2);

	EXPECT_TRUE(dson.get(Key::x6, str));
	EXPECT_EQ(str, str1);
}

TEST(DsonFromFile, SaveAndLoad)
{
	const auto check1 = [](hi::dson::DsonFromFile & dson_on_disk)
	{
		double res_d;
		EXPECT_TRUE(dson_on_disk.get(Key::x1, res_d));
		EXPECT_EQ(res_d, d2);

		std::uint32_t res_ui32;
		EXPECT_TRUE(dson_on_disk.get(Key::x2, res_ui32));
		EXPECT_EQ(res_ui32, ui32);

		std::int64_t res_i64;
		EXPECT_TRUE(dson_on_disk.get(Key::x3, res_i64));
		EXPECT_EQ(res_i64, i64);

		std::uint64_t res_ui64;
		EXPECT_TRUE(dson_on_disk.get(Key::x4, res_ui64));
		EXPECT_EQ(res_ui64, ui64);

		EXPECT_TRUE(dson_on_disk.get(Key::x5, res_d));
		EXPECT_EQ(res_d, d1);

		std::string str;
		EXPECT_TRUE(dson_on_disk.get(Key::x6, str));
		EXPECT_EQ(str, str1);

		EXPECT_TRUE(dson_on_disk.get(Key::x7, str));
		EXPECT_EQ(str, str2);
	};

	{ // prepare file
		hi::dson::NewDson dson;
		dson.emplace(Key::x1, i32);
		dson.emplace(Key::x2, ui32);
		dson.emplace(Key::x3, i64);
		dson.emplace(Key::x4, ui64);
		dson.emplace(Key::x5, d1);
		dson.emplace(Key::x6, str1);
		dson.set_key(12345);

		hi::dson::DsonFromFile dson_on_disk;
		EXPECT_TRUE(dson_on_disk.create(file_path) > 0);
		EXPECT_TRUE(dson_on_disk.upload(dson) > 0);

		dson_on_disk.emplace(Key::x1, d2);
		dson_on_disk.emplace(Key::x7, str2);
		dson_on_disk.save();

		check1(dson_on_disk);
	}

	{ // load and change file
		hi::dson::DsonFromFile dson_on_disk;
		dson_on_disk.open(file_path);

		check1(dson_on_disk);

		dson_on_disk.emplace(Key::x1, str1);
		dson_on_disk.emplace(Key::x2, vec1);
		dson_on_disk.emplace(Key::x6, d1);
		dson_on_disk.emplace(Key::x7, d2);

		dson_on_disk.save();
	}

	{ // load and check changes
		hi::dson::DsonFromFile dson_on_disk;
		dson_on_disk.open(file_path);

		std::string str;
		EXPECT_TRUE(dson_on_disk.get(Key::x1, str));
		EXPECT_EQ(str, str1);

		std::vector<double> vec;
		EXPECT_TRUE(dson_on_disk.get(Key::x2, vec));
		EXPECT_EQ(vec, vec1);

		double res_d;
		EXPECT_TRUE(dson_on_disk.get(Key::x6, res_d));
		EXPECT_EQ(res_d, d1);

		EXPECT_TRUE(dson_on_disk.get(Key::x7, res_d));
		EXPECT_EQ(res_d, d2);
	}
} // SaveAndLoad

TEST(HostAndNetworkOrderTest, SendDsonFromBuff)
{
	const auto check1 = [](hi::dson::NewDson & dson)
	{
		std::int32_t res_i32;
		EXPECT_TRUE(dson.get(Key::x1, res_i32));
		EXPECT_EQ(res_i32, i32);

		std::uint32_t res_ui32;
		EXPECT_TRUE(dson.get(Key::x2, res_ui32));
		EXPECT_EQ(res_ui32, ui32);

		std::int64_t res_i64;
		EXPECT_TRUE(dson.get(Key::x3, res_i64));
		EXPECT_EQ(res_i64, i64);

		std::uint64_t res_ui64;
		EXPECT_TRUE(dson.get(Key::x4, res_ui64));
		EXPECT_EQ(res_ui64, ui64);

		double res_d;
		EXPECT_TRUE(dson.get(Key::x5, res_d));
		EXPECT_EQ(res_d, d1);

		std::string str;
		EXPECT_TRUE(dson.get(Key::x6, str));
		EXPECT_EQ(str, str1);

		std::vector<double> vec;
		EXPECT_TRUE(dson.get(Key::x7, vec));
		EXPECT_EQ(vec, vec1);

		dson.emplace(Key::x1, d2);
		dson.emplace(Key::x7, str2);

		EXPECT_TRUE(dson.get(Key::x7, str));
		EXPECT_EQ(str, str2);

		EXPECT_TRUE(dson.get(Key::x1, res_d));
		EXPECT_EQ(res_d, d2);
	};

	// shared_buf[0] = 0 == можно отправлять, 1 == можно принимать (и наоборот: если можно отправлять, то нельзя
	// принимать)
	std::vector<char> shared_buf(5, 0);

	std::mutex buf_mutex;
	std::condition_variable buf_cv;

	auto sender = [&]
	{
		// Готовим данные
		auto dson = std::make_unique<hi::dson::NewDson>();
		dson->emplace(Key::x1, i32);
		dson->emplace(Key::x2, ui32);
		dson->emplace(Key::x3, i64);
		dson->emplace(Key::x4, ui64);
		dson->emplace(Key::x5, d1);
		dson->emplace(Key::x6, str1);
		dson->emplace(Key::x7, vec1);
		dson->set_key(12345);

		// Готовим загрузчик
		hi::dson::UploaderToBuffHolder uploader;
		uploader.set(std::move(dson));
		const auto upload = [&]() -> hi::result_t
		{
			hi::result_t res = hi::okInProcess;
			if (shared_buf[0])
				return res;
			char * cur_buf = shared_buf.data() + 1;
			auto buf_size = static_cast<std::int32_t>(shared_buf.size() - 1);
			std::int32_t cur_buf_size = buf_size;
			while (hi::okInProcess == res && cur_buf_size)
			{ // тут важно: каждый шаг может наполнять буффер не полностью,
				// а получатель ожидает что если это ещё не конец, то буффер заполнен полностью
				// поэтому цикл
				res = uploader.upload_step(cur_buf, cur_buf_size);
			}

			if (buf_size != cur_buf_size)
			{
				shared_buf[0] = 1;
			}
			else
			{
				// Должно было хоть что-то выгрузиться и размер измениться.
				assert(false);
			}
			buf_cv.notify_one();
			return res;
		};

		// Запускаем выгрузку
		{
			std::unique_lock lk(buf_mutex);
			buf_cv.wait(
				lk,
				[&]() -> bool
				{
					if (hi::okInProcess == upload())
						return false;
					buf_cv.notify_one();
					return true;
				});
		}
	}; // sender

	auto receiver = [&]
	{
		hi::dson::DownloaderFromSharedBuf downloader;
		// Запускаем загрузку
		{
			std::unique_lock lk(buf_mutex);
			buf_cv.wait(
				lk,
				[&]() -> bool
				{
					if (!shared_buf[0])
						return false;
					if (hi::okReady
						== downloader.load(shared_buf.data() + 1, static_cast<std::int32_t>(shared_buf.size() - 1)))
					{
						auto dson = downloader.get_result();
						check1(*dson);
						return true;
					}
					shared_buf[0] = 0;
					buf_cv.notify_one();
					return false;
				});
		}
	}; // receiver

	std::thread th1{std::move(sender)};
	std::thread th2{std::move(receiver)};
	buf_cv.notify_all();
	th1.join();
	buf_cv.notify_all();
	th2.join();
} // SendDsonFromBuff
