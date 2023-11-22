/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_MEM_MAP_PROXY_H
#define THREADS_HIGHWAYS_TOOLS_MEM_MAP_PROXY_H

#if _WIN32
#	include <thread_highways/tools/windows/mem_map_proxy.h>
#else
#	include <thread_highways/tools/posix/mem_map_proxy.h>
#endif

#endif // THREADS_HIGHWAYS_TOOLS_MEM_MAP_PROXY_H
