/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_THREAD_TOOLS_H
#define THREADS_HIGHWAYS_TOOLS_THREAD_TOOLS_H

#if _WIN32
#	include <thread_highways/tools/windows/thread_tools.h>
#elif __ANDROID__
#	include <thread_highways/tools/linux/thread_tools.h>
#elif __linux__
#	include <thread_highways/tools/linux/thread_tools.h>
#elif __mac__
#	include <thread_highways/tools/mac/thread_tools.h>
#else

#endif

#endif // THREADS_HIGHWAYS_TOOLS_THREAD_TOOLS_H
