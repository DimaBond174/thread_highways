/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_LIFETIME_CONTROLLER_H
#define THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_LIFETIME_CONTROLLER_H

#include <thread_highways/dson/dson_from_file.h>

#include <cstdio>
#include <vector>

namespace hi
{
namespace dson
{

class DsonFromFileLifetimeController : public DsonFromFileController
{
public:
	DsonFromFileLifetimeController(std::int32_t max_size, std::string folder)
		: DsonFromFileController(max_size, std::move(folder))
	{
	}

	~DsonFromFileLifetimeController()
	{
		delete_files();
	}

	std::unique_ptr<DsonFromFile> create_new() override
	{
		std::unique_ptr<DsonFromFile> re = std::make_unique<DsonFromFile>();
		auto path = folder_ + std::to_string(++cur_file_id_);
		re->create(path);
		files_.emplace_back(std::move(path));
		return re;
	}

private:
	void delete_files()
	{
		for (auto & it : files_)
		{
			std::remove(it.c_str());
		}
	}

private:
	std::vector<std::string> files_;
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DSON_FROM_FILE_LIFETIME_CONTROLLER_H
