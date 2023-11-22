/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

namespace hi
{
namespace
{

TEST(Future, GetFutureWithFutureChainControl)
{
	const auto highway = hi::make_self_shared<HighWay>();
	auto chain_control = hi::Future<int>::create(
							 []() -> int
							 {
								 return 1;
							 },
							 highway,
							 __FILE__,
							 __LINE__)
							 ->next<std::int64_t>(
								 [](int result) -> std::int64_t
								 {
									 std::int64_t re = result + 20;
									 return re;
								 },
								 __FILE__,
								 __LINE__)
							 ->next<std::string>(
								 [](std::int64_t result) -> std::string
								 {
									 std::string re{"3 + "};
									 re.append(std::to_string(result));
									 return re;
								 },
								 __FILE__,
								 __LINE__);
	auto std_future = chain_control->execute();

	EXPECT_EQ(std_future.get(), "3 + 21");

	highway->destroy();
}

TEST(Future, GetFutureAndDetach)
{
	const auto highway = hi::make_self_shared<HighWay>();
	auto std_future = hi::Future<int>::create(
						  []() -> int
						  {
							  return 1;
						  },
						  highway,
						  __FILE__,
						  __LINE__)
						  ->next<std::int64_t>(
							  [](int result) -> std::int64_t
							  {
								  std::int64_t re = result + 20;
								  return re;
							  },
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [](std::int64_t result) -> std::string
							  {
								  std::string re{"3 + "};
								  re.append(std::to_string(result));
								  return re;
							  },
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	EXPECT_EQ(std_future.get(), "3 + 21");

	highway->destroy();
}

TEST(Future, Detach)
{
	const auto highway = hi::make_self_shared<HighWay>();
	std::promise<std::string> promise;
	auto std_future = promise.get_future();

	hi::Future<int>::create(
		[]() -> int
		{
			return 1;
		},
		highway,
		__FILE__,
		__LINE__)
		->next<std::int64_t>(
			[&](int result) -> std::int64_t
			{
				std::int64_t re = result + 20;
				return re;
			},
			__FILE__,
			__LINE__)
		->next<std::string>(
			[&](std::int64_t result) -> std::string
			{
				std::string re{"3 + "};
				re.append(std::to_string(result));
				promise.set_value(re);
				return re;
			},
			__FILE__,
			__LINE__)
		->detach();

	EXPECT_EQ(std_future.get(), "3 + 21");

	highway->destroy();
}

TEST(Future, DetachWithProtector)
{
	const auto highway = hi::make_self_shared<HighWay>();
	std::promise<std::string> promise;
	auto std_future = promise.get_future();

	auto shared = std::make_shared<bool>();
	auto protector = std::weak_ptr(shared);

	hi::Future<int>::create_with_protector(
		[]() -> int
		{
			return 1;
		},
		protector,
		highway,
		__FILE__,
		__LINE__)
		->next_with_protector<std::int64_t>(
			[&](int result) -> std::int64_t
			{
				std::int64_t re = result + 20;
				return re;
			},
			protector,
			highway,
			__FILE__,
			__LINE__)
		->next_with_protector<std::string>(
			[&](std::int64_t result) -> std::string
			{
				std::string re{"3 + "};
				re.append(std::to_string(result));
				promise.set_value(re);
				return re;
			},
			protector,
			highway,
			__FILE__,
			__LINE__)
		->detach();

	EXPECT_EQ(std_future.get(), "3 + 21");

	highway->destroy();
}

} // namespace
} // namespace hi
