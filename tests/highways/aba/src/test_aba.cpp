/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <future>
#include <vector>

namespace hi
{

template <typename H>
void aba_test(H highway, const size_t load_size, const size_t threads_cnt)
{
	const auto aba_load = [&]
	{
		std::vector<std::future<std::size_t>> futures;
		futures.reserve(load_size);
		for (std::size_t i = 0u; i < load_size; ++i)
		{
			std::promise<std::size_t> prom;

			futures.emplace_back(prom.get_future());
			highway->execute(
				[&, i, p = std::move(prom), executed = bool{false}]() mutable
				{
					EXPECT_FALSE(executed);
					executed = true;
					p.set_value(i * i);
				});
		}

		for (std::size_t i = 0u; i < load_size; ++i)
		{
			const auto expected = i * i;
			EXPECT_EQ(expected, futures[i].get());
		}
	};

	std::vector<std::thread> threads;
	for (std::size_t i = 0u; i < threads_cnt; ++i)
	{
		threads.emplace_back(
			[&]
			{
				aba_load();
			});
	}

	for (std::size_t i = 0u; i < threads_cnt; ++i)
	{
		threads[i].join();
	}

	highway->destroy();
}

struct TestDataHighWayAbaSafe
{
	std::string_view description_;
	std::shared_ptr<hi::HighWayAbaSafe> highway_;
	size_t load_size_;
	size_t threads_cnt_;
};

TestDataHighWayAbaSafe aba_test_data[] = {
	TestDataHighWayAbaSafe{
		"NoTimeControl",
		hi::make_self_shared<hi::HighWayAbaSafe>(100 /*mail_box_capacity*/),
		1000, // load_size
		40 // threads_cnt
	},
};

struct HighWayAbaSafeTest : public ::testing::TestWithParam<TestDataHighWayAbaSafe>
{
};

INSTANTIATE_TEST_SUITE_P(
	HighWayAbaSafeTestInstance,
	HighWayAbaSafeTest,
	::testing::ValuesIn(aba_test_data),
	[](const ::testing::TestParamInfo<TestDataHighWayAbaSafe> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(HighWayAbaSafeTest, AbaTest)
{
	const TestDataHighWayAbaSafe & test_params = GetParam();
	aba_test(test_params.highway_, test_params.load_size_, test_params.threads_cnt_);
}

struct TestDataHighWay
{
	std::string_view description_;
	std::shared_ptr<hi::HighWay> highway_;
	std::uint32_t capacity_;
	size_t load_size_;
	size_t threads_cnt_;
};

TestDataHighWay test_data[] = {
	TestDataHighWay{
		"NoTimeControl",
		hi::make_self_shared<hi::HighWay>(),
		100, // capacity (holders count)
		1000, // load_size
		40 // threads_cnt
	},
};

struct HighWayAbaTest : public ::testing::TestWithParam<TestDataHighWay>
{
};

INSTANTIATE_TEST_SUITE_P(
	HighWayAbaTestInstance,
	HighWayAbaTest,
	::testing::ValuesIn(test_data),
	[](const ::testing::TestParamInfo<TestDataHighWay> & test_info)
	{
		return std::string{test_info.param.description_};
	});

TEST_P(HighWayAbaTest, AbaTest)
{
	const TestDataHighWay & test_params = GetParam();
	test_params.highway_->set_capacity(test_params.capacity_);
	aba_test(test_params.highway_, test_params.load_size_, test_params.threads_cnt_);
}

} // namespace hi
