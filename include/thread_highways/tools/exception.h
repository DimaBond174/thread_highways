/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_EXCEPTION_H
#define THREADS_HIGHWAYS_TOOLS_EXCEPTION_H

#include "call_stack.h"
#include "template_tools.h"

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace hi {

template<typename T>
struct DsonRecord
{
  T data;
  std::uint32_t key;
};

class Exception : public std::exception
{
public:
  Exception () noexcept = default;
  Exception (std::string filename, unsigned int line) noexcept
  : call_stack_{get_call_stack()}
  , filename_{std::move(filename)}
  , line_{line}
  {
  }

  Exception (std::string what, std::string filename, unsigned int line) noexcept
  :  call_stack_{get_call_stack()}
  , what_{std::move(what)}
  , filename_{std::move(filename)}
  , line_{line}
  {
  }

  template<typename ...AddInfo>
  Exception(std::string what, std::string filename, unsigned int line, AddInfo&& ...infos)
  : call_stack_{get_call_stack()}
  , what_{std::move(what)}
  , filename_{std::move(filename)}
  , line_{line}
  {
    add_infoN(std::forward<AddInfo>(infos)...);
  }

  template<typename T, typename ...AddInfo>
  void add_infoN(T&& info, AddInfo ...infos)
  {
    add_info1(std::forward<T>(info));
    if constexpr(sizeof...(infos) > 0)
    {
      add_infoN(std::forward<AddInfo>(infos)...);
    }
    // todo dson_.add(info.key, std::move(info.data))
  }

  void add_info1(std::exception_ptr e)
  {
    if (!e) return;
    try {
      std::rethrow_exception(e);
    } catch (const std::runtime_error &e) {
      what_.append("std::runtime_error: ").append(e.what());
    } catch (const std::exception &e) {
      what_.append("std::exception: ").append(e.what());
    } catch (...) {
      what_.append("unknown exception..");
    }
  }

  const CallStack& call_stack() const noexcept
  {
    return call_stack_;
  }

  const std::string& file_name() const noexcept
  {
    return filename_;
  }

  unsigned int file_line() const noexcept
  {
    return line_;
  }

  const char* what() const noexcept override
  {
    return what_.c_str();
  }

  const std::string& what_as_string() const noexcept
  {
    return what_;
  }

private:
  template<typename Stream>
  friend Stream& operator<<(Stream &os, const Exception& e)
  {
    os << e.filename_ << ':' << e.line_
      << ':' << e.what_as_string();
    //os << "\nAddInfo:\n"; << e.data_;
    //e.data_.print(os);
    os << e.call_stack_;
    return os;
  }

private:
  CallStack call_stack_;
  std::string what_;
  //Dson data_;
  std::string filename_;
  unsigned int line_;
};

using ExceptionHandler = std::function<void(const hi::Exception&)>;

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_EXCEPTION_H
