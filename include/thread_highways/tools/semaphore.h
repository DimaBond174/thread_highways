/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_SEMAPHORE_H
#define THREADS_HIGHWAYS_TOOLS_SEMAPHORE_H

#if __linux__
#	include <thread_highways/tools/linux/semaphore.h>
#else
#	include <thread_highways/tools/default/semaphore.h>
#endif

#endif // THREADS_HIGHWAYS_TOOLS_SEMAPHORE_H
