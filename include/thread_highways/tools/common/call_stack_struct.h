/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_COMMON_CALL_STACK_STRUCT_H
#define THREADS_HIGHWAYS_TOOLS_COMMON_CALL_STACK_STRUCT_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace hi
{

struct StackBinary
{
	std::string binary_name_;
	std::uint64_t load_address_;
};

struct StackFrame
{
	std::string symbol_;
	std::uint64_t address_;
};

struct CallStack
{
	std::vector<StackBinary> binaries_;
	std::vector<StackFrame> frames_;
};

template <typename Stream>
Stream & operator<<(Stream & os, const CallStack & stack)
{
	os << "\n=======================\n"
	   << "Call stack:\n";
	for (const auto & it : stack.frames_)
	{
		os << std::hex << it.address_ << ": " << it.symbol_ << std::endl;
	}
	os << "\n=======================\n"
	   << "Loaded binary:\n";
	for (const auto & it : stack.binaries_)
	{
		os << std::hex << it.load_address_ << ": " << it.binary_name_ << std::endl;
	}
	os << "\n=======================\n"
	   << "Usage:\n"
	   << "addr2line -e where_fail.so address\n"
	   << " where address = address_call_stack - address_loaded_binary\n";
	return os;
}

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_COMMON_CALL_STACK_STRUCT_H
