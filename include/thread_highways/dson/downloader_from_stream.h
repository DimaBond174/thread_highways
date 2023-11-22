/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_STREAM_H
#define THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_STREAM_H

#include <thread_highways/dson/dson_from_buff.h>
#include <thread_highways/dson/dson_from_file.h>
#include <thread_highways/tools/fd_write_read.h>

#include <fstream>
#include <memory>

namespace hi
{
namespace dson
{

// Чтение из stream
struct DownloaderFromStream
{
	/*
	 * @brief load_from_stream
	 * Загрузка из ifstream
	 * @param input - ifstream
	 * @note загрузка происходит разом всего объёма
	 */
	static std::unique_ptr<NewDson> load(std::ifstream & input, DsonFromFileController * dson_from_file_controller = {})
	{
		DsonHeader header;
		load_header(input, header);
		if (header.data_size_ == 0)
		{
			auto re = std::make_unique<NewDson>();
			re->set_key(header.key_);
			return re;
		}

		std::unique_ptr<NewDson> re;
		if (dson_from_file_controller && dson_from_file_controller->max_size() < header.data_size_)
		{
			re = dson_from_file_controller->create_new();
		}
		else
		{
			re = std::make_unique<DsonFromBuff>();
		}

		ICanUploadIntoDson * dson = dynamic_cast<ICanUploadIntoDson *>(re.get());
		if (dson->set_header(header) < 0)
			return {};
		if (load_body(input, re) < 0)
			return {};
		return re;
	}

	static void load_header(std::ifstream & input, DsonHeader & header)
	{
		char header_buf[detail::header_size];
		HI_ASSERT(input.read(header_buf, detail::header_size));
		detail::get(header_buf, header);
		HI_ASSERT(detail::is_dson_header(header));
	}

	static result_t load_body(std::ifstream & input, std::unique_ptr<NewDson> & re)
	{
		ICanUploadIntoDson * dson = dynamic_cast<ICanUploadIntoDson *>(re.get());
		result_t res = okInProcess;
		while (res == okInProcess)
		{
			auto buf_size = dson->max_chunk_size();
			char * buf = dson->upload_with_uploader_buf(buf_size);
			if (!buf)
			{
				throw Exception("upload_with_uploader_buf return nullptr", __FILE__, __LINE__);
			}
			if (!input.read(buf, buf_size))
			{
				throw Exception("std::ifstream input read fail", __FILE__, __LINE__);
			}

			res = dson->set_uploaded(buf_size);
		}
		return res;
	}

}; // DownloaderFromStream

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DOWNLOADER_FROM_STREAM_H
