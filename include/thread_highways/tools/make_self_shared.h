#ifndef MAKE_SELF_SHARED_H
#define MAKE_SELF_SHARED_H

#include <memory>

namespace hi
{

template <typename T, typename... Args>
[[nodiscard]] std::shared_ptr<T> make_self_shared(Args &&... args)
{
	struct SharedHolder : public std::enable_shared_from_this<SharedHolder>
	{
		alignas(T) unsigned char buffer_[sizeof(T)];
		T * ptr_{nullptr};
		std::shared_ptr<T> construct(Args &&... args)
		{
			auto ptr = std::launder(reinterpret_cast<T *>(buffer_));
			auto alias = std::shared_ptr<T>(this->shared_from_this(), ptr);
			::new (ptr) T(alias, std::forward<Args>(args)...);
			ptr_ = ptr;
			return alias;
		}

		SharedHolder()
		{
		}
		~SharedHolder()
		{
			if (ptr_)
			{
				std::destroy_at(ptr_);
			}
		}

		SharedHolder(const SharedHolder &) = delete;
		SharedHolder(SharedHolder &&) = delete;

		SharedHolder & operator=(const SharedHolder &) = delete;
		SharedHolder & operator=(SharedHolder &&) = delete;
	};

	auto holder = std::make_shared<SharedHolder>();
	return holder->construct(std::forward<Args>(args)...);
}

} // namespace hi

#endif // MAKE_SELF_SHARED_H
