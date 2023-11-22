#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

void std_future()
{
	hi::CoutScope scope("std_future()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	// Сохраняю указатель чтобы цепочка существовала
	auto future_chain = hi::Future<std::string>::create(
							[&]() -> std::string
							{
								std::string re{"1"};
								scope.print(std::string{"future1: "}.append(re));
								return re;
							},
							highway,
							__FILE__,
							__LINE__)
							->next<std::string>(
								[&](std::string result) -> std::string
								{
									result.append(",2");
									scope.print(std::string{"future2: "}.append(result));
									return result;
								},
								__FILE__,
								__LINE__)
							->next<std::string>(
								[&](std::string result) -> std::string
								{
									result.append(",3");
									scope.print(std::string{"future3: "}.append(result));
									return result;
								},
								__FILE__,
								__LINE__);

	auto std_future = future_chain->execute();
	auto res = std_future.get();
	scope.print(std::string{"result future: "}.append(res));
	highway->destroy();
} // std_future()

void std_future_with_break_execution()
{
	hi::CoutScope scope("std_future_with_break_execution()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	// Сохраняю указатель чтобы цепочка существовала
	auto future_chain =
		hi::Future<std::string>::create(
			[&]() -> std::string
			{
				std::string re{"1"};
				scope.print(std::string{"std_future_with_break_execution1: "}.append(re));
				return re;
			},
			highway,
			__FILE__,
			__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",2");
					scope.print(std::string{"std_future_with_break_execution2: "}.append(result));
					scope.print(std::string{"std_future_with_break_execution2: going to sleep for 1sec"});
					std::this_thread::sleep_for(std::chrono::seconds(1));
					return result;
				},
				__FILE__,
				__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",3");
					scope.print(std::string{"ERROR if you see this: "}.append(result));
					return result;
				},
				__FILE__,
				__LINE__);

	auto std_future = future_chain->execute();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	future_chain.reset();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	highway->destroy();
} // std_future_with_break_execution()

void detached_future()
{
	hi::CoutScope scope("detached_future()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	auto std_future = hi::Future<std::string>::create(
						  [&]() -> std::string
						  {
							  std::string re{"1"};
							  scope.print(std::string{"detached_future1: "}.append(re));
							  return re;
						  },
						  highway,
						  __FILE__,
						  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",2");
								  scope.print(std::string{"detached_future2: "}.append(result));
								  return result;
							  },
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",3");
								  scope.print(std::string{"detached_future3: "}.append(result));
								  return result;
							  },
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	auto res = std_future.get();
	scope.print(std::string{"result future: "}.append(res));
	highway->destroy();
} // detached_future()

void detached_future_3_highways()
{
	hi::CoutScope scope("detached_future_3_highways()");
	const auto highway1 = hi::make_self_shared<hi::HighWay>();
	const auto highway2 = hi::make_self_shared<hi::HighWay>();
	const auto highway3 = hi::make_self_shared<hi::HighWay>();

	auto std_future = hi::Future<std::string>::create(
						  [&]() -> std::string
						  {
							  std::string re{"1"};
							  scope.print(std::string{"detached_future1: "}.append(re));
							  return re;
						  },
						  highway1,
						  __FILE__,
						  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",2");
								  scope.print(std::string{"detached_future2: "}.append(result));
								  return result;
							  },
							  highway2,
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",3");
								  scope.print(std::string{"detached_future3: "}.append(result));
								  return result;
							  },
							  highway3,
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	auto res = std_future.get();
	scope.print(std::string{"result future: "}.append(res));
	highway1->destroy();
	highway2->destroy();
	highway3->destroy();
} // detached_future_3_highways()

void detached_future_diff_types()
{
	hi::CoutScope scope("detached_future_diff_types()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	auto std_future = hi::Future<int>::create(
						  [&]() -> int
						  {
							  int re{1};
							  scope.print(std::string{"detached_future1: "}.append(std::to_string(re)));
							  return re;
						  },
						  highway,
						  __FILE__,
						  __LINE__)
						  ->next<double>(
							  [&](int result) -> double
							  {
								  double re = result;
								  re += 0.2;
								  scope.print(std::string{"detached_future2: "}.append(std::to_string(re)));
								  return re;
							  },
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [&](double result) -> std::string
							  {
								  std::string re{"3 + "};
								  re.append(std::to_string(result));
								  scope.print(std::string{"detached_future3: "}.append(re));
								  return re;
							  },
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	auto res = std_future.get();
	scope.print(std::string{"result future: "}.append(res));
	highway->destroy();
} // detached_future_diff_types()

void detached_future_with_exception()
{
	hi::CoutScope scope("detached_future_with_exception()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	auto std_future = hi::Future<std::string>::create(
						  [&]() -> std::string
						  {
							  std::string re{"1"};
							  scope.print(std::string{"detached_future1: "}.append(re));
							  throw hi::Exception("Oh, Error!!!", __FILE__, __LINE__);
							  return re;
						  },
						  highway,
						  __FILE__,
						  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",2");
								  scope.print(std::string{"detached_future2: "}.append(result));
								  return result;
							  },
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [&](std::string result) -> std::string
							  {
								  result.append(",3");
								  scope.print(std::string{"detached_future3: "}.append(result));
								  return result;
							  },
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	try
	{
		auto res = std_future.get();
		scope.print(std::string{"result future: "}.append(res));
	}
	catch (const hi::Exception & e)
	{
		scope.print(std::string{"ERROR: "}.append(e.all_info_as_astring()));
	}

	highway->destroy();
} // detached_future_with_exception()

void detached()
{
	hi::CoutScope scope("detached");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	{
		hi::Future<std::string>::create(
			[&]() -> std::string
			{
				std::string re{"1"};
				scope.print(std::string{"detached1: "}.append(re));
				return re;
			},
			highway,
			__FILE__,
			__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",2");
					scope.print(std::string{"detached2: "}.append(result));
					return result;
				},
				__FILE__,
				__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",3");
					scope.print(std::string{"detached3: "}.append(result));
					return result;
				},
				__FILE__,
				__LINE__)
			->detach();
	}
	highway->flush_tasks();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	highway->flush_tasks();
	highway->destroy();
} // detached()

void detached_with_exception()
{
	hi::CoutScope scope("detached_with_exception");
	const auto highway = hi::make_self_shared<hi::HighWay>(
		[&](const hi::Exception & ex)
		{
			// throw ex;
			scope.print(std::string{"ERROR on HighWay: "}.append(ex.all_info_as_astring()));
		});

	{
		hi::Future<std::string>::create(
			[&]() -> std::string
			{
				std::string re{"1"};
				scope.print(std::string{"detached1: "}.append(re));
				return re;
			},
			highway,
			__FILE__,
			__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",2");
					scope.print(std::string{"detached2: "}.append(result));
					throw hi::Exception("Oh, Error!!!", __FILE__, __LINE__);
					return result;
				},
				__FILE__,
				__LINE__)
			->next<std::string>(
				[&](std::string result) -> std::string
				{
					result.append(",3");
					scope.print(std::string{"detached3: "}.append(result));
					return result;
				},
				__FILE__,
				__LINE__)
			->detach();
	}
	highway->flush_tasks();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	highway->flush_tasks();
	highway->destroy();
} // detached_with_exception()

void detached_with_protectors(int check_protect_id)
{
	hi::CoutScope scope("detached_with_protectors");
	std::vector<std::shared_ptr<hi::HighWay>> highways;
	std::vector<std::shared_ptr<bool>> protectors;

	for (int i = 0; i < 5; ++i)
	{
		highways.emplace_back(hi::make_self_shared<hi::HighWay>());
		protectors.emplace_back(std::make_shared<bool>());
	}

	auto chain = hi::Future<std::string>::create_with_protector(
		[&, id = 0]() -> std::string
		{
			std::string re{std::to_string(id)};
			scope.print(std::string{"detached"}.append(std::to_string(id)).append(": ").append(re));
			return re;
		},
		std::weak_ptr<bool>(protectors[0]),
		highways[0],
		__FILE__,
		__LINE__);

	for (int i = 1; i < 5; ++i)
	{
		chain = chain->next_with_protector<std::string>(
			[&, id = i](std::string result) -> std::string
			{
				result.append(",").append(std::to_string(id));
				scope.print(std::string{"detached"}.append(std::to_string(id)).append(": ").append(result));
				return result;
			},
			std::weak_ptr<bool>(protectors[i]),
			highways[i],
			__FILE__,
			__LINE__);
	}

	scope.print(std::string{"reset protector №"}.append(std::to_string(check_protect_id)));
	protectors[check_protect_id].reset();

	std::this_thread::sleep_for(std::chrono::seconds(1));
	for (int i = 0; i < 5; ++i)
	{
		highways[i]->destroy();
	}
} // detached_with_protectors()

void detached_future_diff_types_with_protector()
{
	hi::CoutScope scope("detached_future_diff_types_with_protector()");
	const auto highway = hi::make_self_shared<hi::HighWay>();
	const auto protector = std::weak_ptr(highway);
	auto std_future = hi::Future<int>::create_with_protector(
						  [&]() -> int
						  {
							  int re{1};
							  scope.print(std::string{"detached_future1: "}.append(std::to_string(re)));
							  return re;
						  },
						  protector,
						  highway,
						  __FILE__,
						  __LINE__)
						  ->next_with_protector<double>(
							  [&](int result) -> double
							  {
								  double re = result;
								  re += 0.2;
								  scope.print(std::string{"detached_future2: "}.append(std::to_string(re)));
								  return re;
							  },
							  protector,
							  highway,
							  __FILE__,
							  __LINE__)
						  ->next<std::string>(
							  [&](double result) -> std::string
							  {
								  std::string re{"3 + "};
								  re.append(std::to_string(result));
								  scope.print(std::string{"detached_future3: "}.append(re));
								  return re;
							  },
							  __FILE__,
							  __LINE__)
						  ->execute_and_detach();

	auto res = std_future.get();
	scope.print(std::string{"result future: "}.append(res));
	highway->destroy();
} // detached_future_diff_types_with_protector()

int main(int /* argc */, char ** /* argv */)
{
	std_future();
	std_future_with_break_execution();
	detached_future();
	detached_future_diff_types();
	detached_future_3_highways();
	detached_future_with_exception();
	detached();
	detached_with_exception();

	for (int i = 0; i < 5; ++i)
	{
		detached_with_protectors(i);
	}
	detached_future_diff_types_with_protector();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
