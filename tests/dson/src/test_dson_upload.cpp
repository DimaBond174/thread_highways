/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/dson/include_all.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>

namespace hi::dson
{

struct TestData
{
	template <typename T>
	TestData(std::string_view description, std::shared_ptr<T> from, std::shared_ptr<T> to)
		: description_{description}
		, from_{from}
		, to_{to}
		, tester_{[from, to](IDson & dson, const std::function<void(hi::dson::NewDson &)> & checker)
				  {
					  HI_ASSERT(from->upload(dson) > 0);
					  checker(*from);
					  HI_ASSERT(to->upload(*from) > 0);
					  checker(*to);
				  }}
	{
	}

	std::string_view description_;
	std::shared_ptr<NewDson> from_;
	std::shared_ptr<NewDson> to_;
	std::function<void(IDson &, const std::function<void(hi::dson::NewDson &)> &)> tester_;
};

TestData test_data[] = {
	TestData{"DsonFromBuff", std::make_shared<DsonFromBuff>(), std::make_shared<DsonFromBuff>()},
	TestData{
		"DsonFromFile",
		std::make_shared<DsonFromFile>("FromDsonFromFile.dson"),
		std::make_shared<DsonFromFile>("ToDsonFromFile.dson")},
};

struct DsonUploadTest : public ::testing::TestWithParam<TestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	DsonUploadTestInstance,
	DsonUploadTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestData> & test_info)
	{
		return std::string{test_info.param.description_};
	});

enum class MyKey : std::int32_t
{
	k_uint32 = 1,
	k_int64 = 2,
	k_double = 3,
	k_string = 4,
	k_dson = 5,
};

template <typename ValueType>
void compare(ValueType expected, MyKey key, hi::dson::NewDson & dson)
{
	ValueType val;
	EXPECT_TRUE(dson.get(key, val));
	EXPECT_EQ(val, expected);
}

TEST_P(DsonUploadTest, ChainOfUploads)
{
	const TestData & test_params = GetParam();

	std::uint32_t k_uint32_val{12346u};
	std::int64_t k_int64_val{12347};
	double k_double_val{12348.8};
	std::string k_string_val{"12349.9"};

	hi::dson::NewDson dson;
	dson.set_key_cast(MyKey::k_dson);
	dson.emplace(MyKey::k_uint32, k_uint32_val);
	dson.emplace(MyKey::k_int64, k_int64_val);
	dson.emplace(MyKey::k_double, k_double_val);
	dson.emplace(MyKey::k_string, k_string_val);
	const auto size = dson.all_size();

	const auto check = [&](hi::dson::NewDson & dson)
	{
		EXPECT_EQ(dson.obj_key(), static_cast<std::int32_t>(MyKey::k_dson));
		EXPECT_EQ(dson.all_size(), size);
		compare(k_uint32_val, MyKey::k_uint32, dson);
		compare(k_int64_val, MyKey::k_int64, dson);
		compare(k_double_val, MyKey::k_double, dson);
		compare(k_string_val, MyKey::k_string, dson);
	};

	test_params.tester_(dson, check);
}

} // namespace hi::dson
