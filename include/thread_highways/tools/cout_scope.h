#ifndef CoutScope_H
#define CoutScope_H

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace hi
{

/**
 * @brief The CoutScope struct
 * A tool for displaying test information on the screen in the correct order.
 *  As bonus measures the time that the scope was running.
 */
struct CoutScope
{
	CoutScope(std::string example_name)
		: scope_start_time_{std::chrono::steady_clock::now()}
		, example_name_{std::move(example_name)}
	{
		std::cout << "\n---------------------------\n=>Started: " << example_name_
				  << ", thread id: " << std::this_thread::get_id() << "\n\n";
	}
	CoutScope(const CoutScope &) = delete;
	CoutScope & operator=(const CoutScope &) = delete;
	CoutScope(CoutScope &&) = delete;
	CoutScope & operator=(CoutScope &&) = delete;

	~CoutScope()
	{
		std::lock_guard lg{mutex_};
		const auto scope_finish_time = std::chrono::steady_clock::now();
		const auto duration = scope_finish_time - scope_start_time_;
		std::cout << "\n=>Finished: " << example_name_ << ", execution time microseconds:"
				  << (std::chrono::duration_cast<std::chrono::microseconds>(duration).count())
				  << "\n---------------------------\n";
	}

	/**
	 * @brief print
	 * Printing a text indicating the thread number where it came from
	 * @param str - text
	 */
	void print(const std::string & str)
	{
		std::lock_guard lg{mutex_};
		std::cout << "thread[" << std::this_thread::get_id() << "]: " << str << std::endl;
	}

	const std::chrono::time_point<std::chrono::steady_clock> scope_start_time_;
	const std::string example_name_;
	std::mutex mutex_;
};

} // namespace hi

#endif // CoutScope_H
