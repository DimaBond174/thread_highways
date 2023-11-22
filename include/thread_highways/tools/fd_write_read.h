/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_FD_WRITE_READ_H
#define THREADS_HIGHWAYS_TOOLS_FD_WRITE_READ_H

#if _WIN32
#	include <thread_highways/tools/windows/fd_write_read.h>
#else
#	include <thread_highways/tools/posix/fd_write_read.h>
#endif

#endif // THREADS_HIGHWAYS_TOOLS_FD_WRITE_READ_H
