/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_H
#define THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_H

#include <thread_highways/dson/detail/types_map.h>

#include <optional>

namespace hi
{
namespace dson
{
namespace detail
{

inline std::int32_t size(const bool)
{
	return static_cast<std::int32_t>(sizeof(char));
}

inline std::int32_t size(const char)
{
	return static_cast<std::int32_t>(sizeof(char));
}

inline std::int32_t size(const std::int8_t)
{
	return static_cast<std::int32_t>(sizeof(std::int8_t));
}

inline std::int32_t size(const std::uint8_t)
{
	return static_cast<std::int32_t>(sizeof(std::uint8_t));
}

inline std::int32_t size(const std::int16_t)
{
	return static_cast<std::int32_t>(sizeof(std::int16_t));
}

inline std::int32_t size(const std::uint16_t)
{
	return static_cast<std::int32_t>(sizeof(std::uint16_t));
}

inline std::int32_t size(const std::int32_t)
{
	return static_cast<std::int32_t>(sizeof(std::int32_t));
}

inline std::int32_t size(const std::uint32_t)
{
	return static_cast<std::int32_t>(sizeof(std::uint32_t));
}

inline std::int32_t size(const std::int64_t)
{
	return static_cast<std::int32_t>(sizeof(std::int64_t));
}

inline std::int32_t size(const std::uint64_t)
{
	return static_cast<std::int32_t>(sizeof(std::uint64_t));
}

inline std::int32_t size(const double)
{
	return detail::double_size;
}

inline std::int32_t size(const std::string & val)
{
	return static_cast<std::int32_t>(val.size());
}

inline std::int32_t size(const std::vector<std::int8_t> & val)
{
	return static_cast<std::int32_t>(val.size());
}

inline std::int32_t size(const std::vector<std::uint8_t> & val)
{
	return static_cast<std::int32_t>(val.size());
}

inline std::int32_t size(const std::vector<std::int16_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::int16_t));
}

inline std::int32_t size(const std::vector<std::uint16_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::uint16_t));
}

inline std::int32_t size(const std::vector<std::int32_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::int32_t));
}

inline std::int32_t size(const std::vector<std::uint32_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::uint32_t));
}

inline std::int32_t size(const std::vector<std::int64_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::int64_t));
}

inline std::int32_t size(const std::vector<std::uint64_t> & val)
{
	return static_cast<std::int32_t>(val.size() * sizeof(std::uint64_t));
}

inline std::int32_t size(const std::vector<double> & val)
{
	return static_cast<std::int32_t>(val.size()) * detail::double_size;
}

inline std::int32_t size(const BufUChar & val)
{
	return val.size();
}

// General template
template <typename T>
struct IsDsonNumber
{
	enum
	{
		result = false
	};
};

// Template specializations for each fundamental type
template <>
struct IsDsonNumber<char>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<bool>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::int8_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::uint8_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::int16_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::uint16_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::int32_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::uint32_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::int64_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<std::uint64_t>
{
	enum
	{
		result = true
	};
};

template <>
struct IsDsonNumber<double>
{
	enum
	{
		result = true
	};
};

template <typename T, bool>
struct GetObj;

template <typename T>
struct GetObj<T, true>
{
    static void get(const char * buf, const std::int32_t /*size*/, bool with_header, T & val)
	{
        if (with_header) {
            detail::get(buf + detail::header_size, val);
        } else {
            detail::get(buf, val);
        }
	}

	static bool buf_view(const T & val, BufUCharView & result)
	{
		result.init(&val, sizeof(T));
		return true;
	}
};

template <typename T>
struct GetObj<T, false>
{
    static void get(const char * buf, const std::int32_t size, bool with_header, T & val)
	{
        if (with_header) {
            DsonHeader header;
            detail::get_and_shift(buf, header);
            detail::get(buf, header.data_size_, val);
        } else {
            detail::get(buf, size, val);
        }
	}

	static bool buf_view(const T & val, BufUCharView & result)
	{
		result.init(val.data(), size(val));
		return true;
	}
};

template <typename T>
inline result_t upload_to(const T & val, const std::int32_t at, detail::IUploader & uploader)
{
	// Буффер под выгрузку должен позволять выгружать объект полностью, поэтому
	// не должно возникать ситуации когда что-то надо дозагрузить не с начала.
	// Например: double делает сложные расчётны для выгрузки и делать их несколько раз или хранить результат - накладно
	assert(at == 0);
	auto uploaded = size(val);
	auto buf = uploader.upload_with_uploader_buf(uploaded);
	if (!buf)
		return eNoMemory;
	put(buf, val);
	return uploader.set_uploaded(uploaded);
}

template <typename T>
inline result_t upload_to_from_local_buf(const T & val, const std::int32_t at, detail::IUploader & uploader)
{
	return uploader.upload_chunk(
		reinterpret_cast<const char *>(val.data()) + at,
		static_cast<std::int32_t>(val.size()) - at);
}

inline result_t upload_to(const std::string & val, const std::int32_t at, detail::IUploader & uploader)
{
	return upload_to_from_local_buf(val, at, uploader);
}

inline result_t upload_to(const std::vector<std::int8_t> & val, const std::int32_t at, detail::IUploader & uploader)
{
	return upload_to_from_local_buf(val, at, uploader);
}

inline result_t upload_to(const std::vector<std::uint8_t> & val, const std::int32_t at, detail::IUploader & uploader)
{
	return upload_to_from_local_buf(val, at, uploader);
}

inline result_t upload_to(const BufUChar & val, const std::int32_t at, detail::IUploader & uploader)
{
	return upload_to_from_local_buf(val, at, uploader);
}

inline result_t upload_to(const BufUCharView & val, const std::int32_t at, detail::IUploader & uploader)
{
	return upload_to_from_local_buf(val, at, uploader);
}

} // namespace detail

template <typename Number>
constexpr bool is_dson_number()
{
	return detail::IsDsonNumber<Number>::result;
}

template <typename T>
class DsonObj : public IObjView
{
public:
	DsonObj(Key key, T obj)
		: obj_{std::move(obj)}
	{
		header_.key_ = key;
		header_.data_size_ = detail::size(obj_);
		header_.data_type_ = detail::types_map<T>::value;
	}

	DsonObj(const DsonObj &) = delete;
	DsonObj(DsonObj && other)
	{
		move_from_other(std::move(other));
	}

	DsonObj & operator=(const DsonObj &) = delete;
	DsonObj & operator=(DsonObj && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		move_from_other(std::move(other));
		return *this;
	}

	void get(T & val)
	{
		val = obj_;
	}

public: // IDsonObjView
	std::int32_t data_type() override
	{
		return header_.data_type_;
	}

	std::int32_t data_size() override
	{
		return header_.data_size_;
	}

	Key obj_key() override
	{
		return header_.key_;
	}

    bool buf_view(BufUCharView & result, bool& with_header) override
	{
        with_header = false;
        return detail::GetObj<T, detail::IsDsonNumber<T>::result>::buf_view(obj_, result);
	}

	result_t upload_to(const std::int32_t at, detail::IUploader & uploader) override
	{
		if (at < detail::header_size)
		{
			auto buf = uploader.upload_with_uploader_buf(detail::header_size);
			if (!buf)
				return eNoMemory;
			detail::put(buf, header_);
			return uploader.set_uploaded(detail::header_size);
		}
		return detail::upload_to(obj_, at - detail::header_size, uploader);
	}

private:
	void move_from_other(DsonObj && other)
	{
		obj_ = std::move(other.obj_);
		header_ = other.header_;
	}

private:
	DsonHeader header_;
	T obj_;
};

class DsonObjView : public IObjView
{
public:
	DsonObjView(char * buf)
		: buf_{buf}
	{
		assert(buf_);
	}

	DsonObjView(const DsonObjView & other)
	{
		copy_from_other(other);
	}

	DsonObjView(DsonObjView && other)
	{
		copy_from_other(other);
	}

	DsonObjView & operator=(const DsonObjView & other)
	{
		if (&other == this)
		{
			return *this;
		}

		copy_from_other(other);
		return *this;
	}

	DsonObjView & operator=(DsonObjView && other) noexcept
	{
		if (&other == this)
		{
			return *this;
		}

		copy_from_other(other);
		return *this;
	}

	template <typename T>
	void get(T & val)
	{
        detail::GetObj<T, detail::IsDsonNumber<T>::result>::get(buf_, 0, true, val);
	}

public: // IDsonObjView
	std::int32_t data_type() override
	{
		return detail::get_data_type(buf_);
	}

	std::int32_t data_size() override
	{
		return detail::get_data_size(buf_);
	}

	Key obj_key() override
	{
		return detail::get_key(buf_);
	}

    bool buf_view(BufUCharView & result, bool& with_header) override
	{
		if (!buf_)
			return false;
        with_header = false;
		result.init(buf_ + detail::header_size, static_cast<std::size_t>(detail::get_data_size(buf_)));
		return true;
	}

	result_t upload_to(const std::int32_t at, detail::IUploader & uploader) override
	{
		auto size = detail::get_data_size(buf_) + detail::header_size;
		HI_ASSERT(size > at);
		// if (size <= at) return eWrongParams;
		return uploader.upload_chunk(buf_ + at, size - at);
	}

private:
	void copy_from_other(const DsonObjView & other)
	{
		buf_ = other.buf_;
	}

private:
	char * buf_{nullptr};
};

} // namespace dson
} // namespace hi

#endif // THREADS_HIGHWAYS_DSON_DETAIL_DSON_OBJ_H
