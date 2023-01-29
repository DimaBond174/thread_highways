/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_CALL_STACK_H
#define THREADS_HIGHWAYS_TOOLS_CALL_STACK_H

#if _WIN32
#	include <thread_highways/tools/windows/call_stack.h>
#elif __ANDROID__
#	include <thread_highways/tools/android/call_stack.h>
#elif __linux__
#	include <thread_highways/tools/linux/call_stack.h>
#else
#	include <thread_highways/tools/default/call_stack.h>
#endif

namespace hi {


} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_CALL_STACK_H
