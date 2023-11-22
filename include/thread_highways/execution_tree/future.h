/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_FUTURE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_FUTURE_H

#include <thread_highways/channels/i_subscription.h>

#include <future>
#include <optional>

namespace hi
{

template <typename Result>
struct ResultAndException
{
	std::optional<Result> result_;
	std::optional<Exception> exception_;
};

/*
 next == готовлю следующую фьючу в логику которой включаю эту
 execute == отдаю std::future
 detach == ничего не возвращаю, в next заворачиваю лямбду где захватываю shared на эту Future

 Цепочка держится на двух указателях в одну сторону: подписка и захват в лямбду предыдущего узла.
 Зацикливания не происходит так как в обратную сторону только weak указатели и удаление последнего
 узла в цепочке вызывает разрушение всей цепи.

 ВАЖНО: в момент создания через метод create логика выполнения Future уже отправляется на highway
 (== построение цепочки фьюч может происходить уже на фоне готового результата и это нормально)
*/
template <typename Result>
struct Future
{

	// убрать в private
	Future(std::weak_ptr<Future<Result>> self, HighWayProxyPtr highway)
		: self_{std::move(self)}
		, highway_{std::move(highway)}
	{
	}

	template <typename Logic>
	static std::shared_ptr<Future<Result>> create(
		Logic && logic,
		HighWayProxyPtr highway,
		const char * filename,
		unsigned int line)
	{
		auto future = make_self_shared<Future<Result>>(std::move(highway));
		future->highway_->execute(
			[logic = std::move(logic), future = std::weak_ptr<Future<Result>>{future}]() mutable
			{
				if (auto alive = future.lock())
				{
					ResultAndException<Result> re;
					try
					{
						re.result_ = logic();
					}
					catch (Exception e)
					{
						re.exception_ = std::move(e);
					}
					catch (...)
					{
						re.exception_ = Exception{
							"Future<Result>.Logic() catch(...):",
							__FILE__,
							__LINE__,
							std::current_exception()};
					}
					alive->publish(std::move(re));
				}
			},
			filename,
			line);
		return future;
	} // create

	template <typename Logic>
	static std::shared_ptr<Future<Result>> create(
		Logic && logic,
		std::shared_ptr<HighWay> highway,
		const char * filename,
		unsigned int line)
	{
		return create(std::move(logic), make_proxy(std::move(highway)), filename, line);
	} // create

	template <typename Logic, typename Protector>
	static std::shared_ptr<Future<Result>> create_with_protector(
		Logic && logic,
		Protector protector,
		HighWayProxyPtr highway,
		const char * filename,
		unsigned int line)
	{
		auto future = make_self_shared<Future<Result>>(std::move(highway));
		future->highway_->execute(
			[logic = std::move(logic), future = std::weak_ptr<Future<Result>>{future}]() mutable
			{
				if (auto alive = future.lock())
				{
					ResultAndException<Result> re;
					try
					{
						re.result_ = logic();
					}
					catch (Exception e)
					{
						re.exception_ = std::move(e);
					}
					catch (...)
					{
						re.exception_ = Exception{
							"Future<Result>.Logic() catch(...):",
							__FILE__,
							__LINE__,
							std::current_exception()};
					}
					alive->publish(std::move(re));
				}
			},
			std::move(protector),
			filename,
			line);
		return future;
	} // create_with_protector

	template <typename Logic, typename Protector>
	static std::shared_ptr<Future<Result>> create_with_protector(
		Logic && logic,
		Protector protector,
		std::shared_ptr<HighWay> highway,
		const char * filename,
		unsigned int line)
	{
		return create_with_protector(
			std::move(logic),
			std::move(protector),
			make_proxy(std::move(highway)),
			filename,
			line);
	} // create_with_protector

	// Необходимо хранить указатель на this последний узел цепочки фьюч чтобы вычисления продолжались.
	// Чтобы остановить вычисления можно просто удалить узел с которого вызывается этот метод execute()
	std::future<Result> execute()
	{
		std::promise<Result> promise;
		auto re = promise.get_future();

		self_subscribe(create_subscription<ResultAndException<Result>>(
			[promise = std::move(promise)](ResultAndException<Result> result) mutable
			{
				if (result.exception_.has_value())
				{
					promise.set_exception(std::make_exception_ptr(result.exception_.value()));
					return;
				}
				else if (result.result_.has_value())
				{
					promise.set_value(std::move(*result.result_));
				}
				else
				{
					assert(false); // result or exception must be
				}
			}));

		return re;
	}

	// Можно хранить только std::future<Result>, остановить выполнение цепочки невозможно
	std::future<Result> execute_and_detach()
	{
		std::promise<Result> promise;
		auto re = promise.get_future();
		self_subscribe(create_subscription<ResultAndException<Result>>(
			[promise = std::move(promise), protector = self_.lock()](ResultAndException<Result> result) mutable
			{
				if (result.exception_.has_value())
				{
					promise.set_exception(std::make_exception_ptr(result.exception_.value()));
					return;
				}
				else if (result.result_.has_value())
				{
					promise.set_value(std::move(*result.result_));
				}
				else
				{
					assert(false); // result or exception must be
				}
				// Удаление цепочки фьюч:
				protector.reset();
			}));

		return re;
	}

	// Цепочка Future будет выполняться независимо (запустил и забыл)
	void detach()
	{
		self_subscribe(create_subscription<ResultAndException<Result>>(
			[protector = self_.lock()](ResultAndException<Result> result) mutable
			{
				if (result.exception_.has_value())
				{
					throw *result.exception_;
				}
				// Удаление цепочки фьюч:
				protector.reset();
			}));
	}

	template <typename NextResult, typename NextLogic>
	std::shared_ptr<Future<NextResult>> next(
		NextLogic && logic,
		HighWayProxyPtr highway,
		const char * filename,
		unsigned int line)
	{
		auto future = make_self_shared<Future<NextResult>>(std::move(highway));
		auto subscription = create_subscription<ResultAndException<Result>>(
			[&,
			 logic = std::move(logic),
			 prev = self_.lock(),
			 future = std::weak_ptr<Future<NextResult>>{future},
			 filename,
			 line](ResultAndException<Result> result) mutable
			{
				if (auto alive = future.lock())
				{
					// Перепланирую на потоке следующего узла
					alive->highway_->execute(
						[logic = std::move(logic), result = std::move(result), future]() mutable
						{
							if (auto alive = future.lock())
							{
								ResultAndException<NextResult> re;
								if (result.exception_)
								{
									re.exception_ = result.exception_;
								}
								else if (result.result_)
								{
									try
									{
										re.result_ = logic(std::move(*result.result_));
									}
									catch (Exception e)
									{
										re.exception_ = std::move(e);
									}
									catch (...)
									{
										re.exception_ = Exception{
											"Future<Result>.Logic() catch(...):",
											__FILE__,
											__LINE__,
											std::current_exception()};
									}
								}
								else
								{
									assert(false); // result or exception must be
								}
								alive->publish(std::move(re));
							}
						},
						filename,
						line);
				}
				// Удаление цепочки фьюч:
				prev.reset();
			});
		subscribe(subscription);
		future->subscription_to_prev_ = std::move(subscription);
		return future;
	}

	template <typename NextResult, typename NextLogic>
	std::shared_ptr<Future<NextResult>> next(NextLogic && logic, const char * filename, unsigned int line)
	{
		return next<NextResult, NextLogic>(std::move(logic), highway_, filename, line);
	}

	template <typename NextResult, typename NextLogic>
	std::shared_ptr<Future<NextResult>> next(
		NextLogic && logic,
		std::shared_ptr<HighWay> highway,
		const char * filename,
		unsigned int line)
	{
		return next<NextResult, NextLogic>(std::move(logic), make_proxy(std::move(highway)), filename, line);
	}

	template <typename NextResult, typename NextLogic, typename Protector>
	std::shared_ptr<Future<NextResult>> next_with_protector(
		NextLogic && logic,
		Protector protector,
		HighWayProxyPtr highway,
		const char * filename,
		unsigned int line)
	{
		auto future = make_self_shared<Future<NextResult>>(std::move(highway));
		auto subscription = create_subscription<ResultAndException<Result>>(
			[&,
			 logic = std::move(logic),
			 protector = std::move(protector),
			 prev = self_.lock(),
			 future = std::weak_ptr<Future<NextResult>>{future},
			 filename,
			 line](ResultAndException<Result> result) mutable
			{
				if (auto alive = future.lock())
				{
					// Перепланирую на потоке следующего узла
					alive->highway_->execute(
						[logic = std::move(logic), result = std::move(result), future]() mutable
						{
							if (auto alive = future.lock())
							{
								ResultAndException<NextResult> re;
								if (result.exception_)
								{
									re.exception_ = result.exception_;
								}
								else if (result.result_)
								{
									try
									{
										re.result_ = logic(std::move(*result.result_));
									}
									catch (Exception e)
									{
										re.exception_ = std::move(e);
									}
									catch (...)
									{
										re.exception_ = Exception{
											"Future<Result>.Logic() catch(...):",
											__FILE__,
											__LINE__,
											std::current_exception()};
									}
								}
								else
								{
									assert(false); // result or exception must be
								}
								alive->publish(std::move(re));
							}
						},
						std::move(protector),
						filename,
						line);
				}
				// Удаление цепочки фьюч:
				prev.reset();
			});
		subscribe(subscription);
		future->subscription_to_prev_ = std::move(subscription);
		return future;
	} // next_with_protector

	template <typename NextResult, typename NextLogic, typename Protector>
	std::shared_ptr<Future<NextResult>> next_with_protector(
		NextLogic && logic,
		Protector protector,
		std::shared_ptr<HighWay> highway,
		const char * filename,
		unsigned int line)
	{
		return next_with_protector<NextResult, NextLogic, Protector>(
			std::move(logic),
			std::move(protector),
			make_proxy(std::move(highway)),
			filename,
			line);
	} // next_with_protector

	void publish(ResultAndException<Result> result)
	{
		if (subscription_from_next_)
		{
			if (auto alive = subscription_from_next_->lock())
			{
				alive->send(std::move(result));
			}
			return;
		}
		result_.emplace(std::move(result));
	}

	void self_subscribe(std::shared_ptr<ISubscription<ResultAndException<Result>>> subscription)
	{
		highway_->execute(
			[&, subscription = std::move(subscription)]() mutable
			{
				if (result_)
				{
					subscription->send(std::move(*result_));
					return;
				}
				subscription_from_next_ = subscription;
				subscription_to_self_ = std::move(subscription);
			},
			self_,
			__FILE__,
			__LINE__);
	}

	void subscribe(std::weak_ptr<ISubscription<ResultAndException<Result>>> subscription)
	{
		highway_->execute(
			[&, subscription = std::move(subscription)]() mutable
			{
				if (result_)
				{
					if (auto alive = subscription.lock())
					{
						alive->send(std::move(*result_));
					}
					return;
				}
				subscription_from_next_ = std::move(subscription);
			},
			self_,
			__FILE__,
			__LINE__);
	}

	const std::weak_ptr<Future<Result>> self_;
	const HighWayProxyPtr highway_;
	std::optional<std::weak_ptr<ISubscription<ResultAndException<Result>>>> subscription_from_next_;
	std::shared_ptr<void> subscription_to_prev_;
	std::shared_ptr<void> subscription_to_self_;
	std::optional<ResultAndException<Result>> result_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_FUTURE_H
