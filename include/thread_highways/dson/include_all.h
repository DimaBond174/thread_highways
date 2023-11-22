/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_INCLUDE_ALL_H
#define THREADS_HIGHWAYS_DSON_INCLUDE_ALL_H

#include <thread_highways/dson/downloader_from_fd.h>
#include <thread_highways/dson/downloader_from_shared_buf.h>
#include <thread_highways/dson/downloader_from_stream.h>

#include <thread_highways/dson/dson_edit_controller.h>

#include <thread_highways/dson/uploader_to_buff.h>
#include <thread_highways/dson/uploader_to_fd.h>
#include <thread_highways/dson/uploader_to_stream.h>

//#include <memory>

// namespace hi {
// namespace dson {

// std::unique_ptr<hi::dson::IDson> move_to_unique_ptr(IDson *dson)
//{
//     if (!dson) return {};
//     if (auto ptr = dynamic_cast<NewDson *>(dson))
//     {
//         return std::make_unique<NewDson>(std::move(*ptr));
//     }
//     if (auto ptr = dynamic_cast<DsonFromBuff *>(dson))
//     {
//         return std::make_unique<DsonFromBuff>(std::move(*ptr));
//     }
//     if (auto ptr = dynamic_cast<DsonFromFile *>(dson))
//     {
//         return std::make_unique<DsonFromFile>(std::move(*ptr));
//     }

//    auto res = std::make_unique<DsonFromBuff>();
//    if (ok == res->upload(*dson)) return res;
//    return {};
//}

//} // namespace dson
//} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_INCLUDE_ALL_H
