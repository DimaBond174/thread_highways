/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_LOGGER_H
#define THREADS_HIGHWAYS_TOOLS_LOGGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace hi
{

/**
 * @brief The Logger struct
 * Common Interface of any injected logging
 */
struct Logger
{
	virtual ~Logger() = default;
	virtual void log(const std::string &) = 0;
};

using LoggerPtr = std::unique_ptr<Logger>;
using InternalLogger = std::function<void(const std::string &, const std::string &, unsigned int)>;

namespace
{

/**
 * An example of creating a simple logger that adds information about
 *  the current time and current thread to the record
 * @param sink - string output object
 * @return unique pointer to logger
 *
 * Usage example:
 * https://github.com/DimaBond174/thread_highways/blob/main/examples/highways/monitoring/src/monitoring.cpp#L14
 */
template <typename Sink>
LoggerPtr create_default_logger(Sink && sink)
{
	class DefaultLogger : public Logger
	{
	public:
		DefaultLogger(Sink && sink)
			: sink_{std::move(sink)}
		{
		}

		void log(const std::string & msg) override
		{
			using namespace std::chrono;
			std::stringstream ss;
			ss << "{\"milliseconds_since_epoch\":"
			   << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
			   << ",\"thread\":" << std::this_thread::get_id() << ",\"msg\":" << msg << "}\n";
			sink_(ss.str());
		}

	private:
		Sink sink_;
	};

	return std::make_unique<DefaultLogger>(std::move(sink));
} // create_default_logger

/**
 * An example of creating a buffered logger that adds information about
 *  the current time and current thread to the record
 * @param sink - string output object
 * @param buffer_size - the minimum size that a text must reach before it is sent
 * @return unique pointer to logger
 */
template <typename Sink>
LoggerPtr create_buffered_logger(Sink && sink, const std::uint32_t buffer_size = 0)
{

	class BufferedLogger : public Logger
	{
	public:
		BufferedLogger(Sink && sink, const std::uint32_t buffer_size)
			: sink_{std::move(sink)}
			, buffer_size_{buffer_size}
		{
		}

		~BufferedLogger()
		{
			std::lock_guard lg{buffer_guard_};
			flush();
		}

		void log(const std::string & msg) override
		{
			using namespace std::chrono;
			std::stringstream ss;
			ss << "{\"milliseconds_since_epoch\":"
			   << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
			   << ",\"thread\":" << std::this_thread::get_id() << ",\"msg\":" << msg << "}\n";
			{
				std::lock_guard lg{buffer_guard_};
				buffer_.append(ss.str());
				if (static_cast<std::uint32_t>(buffer_.size()) > buffer_size_)
				{
					flush();
				}
			}
		}

	private:
		void flush()
		{
			sink_(buffer_);
			buffer_.clear();
		}

	private:
		Sink sink_;
		const std::uint32_t buffer_size_;
		std::string buffer_;
		std::mutex buffer_guard_;
	};

	return std::make_unique<BufferedLogger>(std::move(sink), buffer_size);
} // create_buffered_logger

/**
 * @brief The SharedLogger class
 * Cool crutch how to make a unique shared pointer from unique pointer.
 * It is better to use with BufferedLogger together as it already has a mutex inside
 */
class SharedLogger
{
public:
	SharedLogger(LoggerPtr && logger)
		: logger_{std::make_shared<LoggerPtr>(std::move(logger))}
	{
	}

	void log(const std::string & msg)
	{
		(*logger_)->log(msg);
	}

	LoggerPtr get()
	{
		struct LoggerInstance : public Logger
		{
			LoggerInstance(std::shared_ptr<LoggerPtr> logger)
				: logger_{std::move(logger)}
			{
			}

			void log(const std::string & msg) override
			{
				(*logger_)->log(msg);
			}

		private:
			std::shared_ptr<LoggerPtr> logger_;
		};

		return std::make_unique<LoggerInstance>(logger_);
	}

	LoggerPtr operator*()
	{
		return get();
	}

private:
	std::shared_ptr<LoggerPtr> logger_;
};

} // namespace

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_LOGGER_H
