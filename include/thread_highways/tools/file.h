/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_TOOLS_FILE_TOOLS_H
#define THREADS_HIGHWAYS_TOOLS_FILE_TOOLS_H

#include <thread_highways/tools/exception.h>

#include <sys/stat.h>

#include <string>

namespace hi
{

inline void create_folder(const std::string & path)
{
#if defined(Windows)
	if (_mkdir(_path) != 0)
	{
#else
	if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) != 0)
	{
#endif
		HI_ASSERT_INFO(EEXIST == errno, path);
	}
}

inline void create_folders(std::string path)
{
	HI_ASSERT(false); // Алгоритм не совместим с utf8
	const size_t len = path.length();
	HI_ASSERT(len < PATH_MAX);
	for (size_t i = 0u; i < len; ++i)
	{
		const char ch = path[i];
		if ('/' != ch && '\\' != ch)
			continue;
		path[i] = '\0';
		create_folder(path);
		path[i] = '/';
	}
	create_folder(path);
}

inline void create_folders_for_file(const std::string & path)
{
	size_t found = path.find_last_of("/\\");
	create_folders(path.substr(0u, found));
}

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_FILE_TOOLS_H
