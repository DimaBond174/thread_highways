#ifndef THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYPARAMS_H
#define THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYPARAMS_H

#include <thread_highways/tools/exception.h>

#include <chrono>
#include <memory>
#include <string>

namespace hi
{

struct HighWayParams
{
	HighWayParams(
		ExceptionHandler exception_handler =
			[](const hi::Exception & ex)
		{
			throw ex;
		},
		std::string highway_name = "HighWay",
		std::chrono::milliseconds max_task_execution_time = {},
		uint32_t workers = 0)
		: exception_handler_{std::move(exception_handler)}
		, highway_name_{std::move(highway_name)}
		, max_task_execution_time_{max_task_execution_time}
		, workers_{workers}
	{
	}

	const ExceptionHandler exception_handler_;
	const std::string highway_name_;

	// для шедулера параметр не нужен т.к. он сам считает сколько можно проспать до следующего старта
	// а задачи приходят через семафор без сна

	// если задан, то будет контроль времени исполнения - всё что дольше попадёт в exception_handler_
	const std::chrono::milliseconds max_task_execution_time_;

	// количество вспомогательных рабочих потоков
	// если задано, то MainSingleThread не будет брать в работу задачи добавленные через PostOnWorkerThread
	const uint32_t workers_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_HIGHWAYS_HIGHWAYPARAMS_H
