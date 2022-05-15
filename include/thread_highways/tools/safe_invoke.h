/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef SAFE_INVOKE_H
#define SAFE_INVOKE_H

#include <functional>
#include <memory>
#include <type_traits>

namespace hi
{

template <typename Fun, typename Protector, typename... Args>
void safe_invoke_void(Fun && fun, Protector & protector, Args &&... args)
{
	if constexpr (std::is_invocable_v<Fun, Args...>)
	{
		if (auto lock = protector.lock())
		{
			std::invoke(std::forward<Fun>(fun), std::forward<Args>(args)...);
		}
	}
	else
	{
		if (auto lock = protector.lock())
		{
			std::invoke(std::forward<Fun>(fun), *lock, std::forward<Args>(args)...);
		}
	}
}

template <typename Fun, typename Protector, typename... Args>
constexpr std::shared_ptr<std::invoke_result_t<Fun, Args...>> safe_invoke_for_result(
	Fun && fun,
	Protector & protector,
	Args &&... args)
{
	if constexpr (std::is_invocable_v<Fun, Args...>)
	{
		if (auto lock = protector.lock())
		{
			return std::invoke(std::forward<Fun>(fun), std::forward<Args>(args)...);
		}
	}
	else
	{
		if (auto lock = protector.lock())
		{
			return std::invoke(std::forward<Fun>(fun), *lock, std::forward<Args>(args)...);
		}
	}
	return {};
}

namespace detail
{
template <class T>
decltype(static_cast<void>(*std::declval<T>()), std::true_type{}) can_be_dereferenced_impl(int);

template <class>
std::false_type can_be_dereferenced_impl(...);
} // namespace detail

template <class T>
struct can_be_dereferenced : decltype(detail::can_be_dereferenced_impl<T>(0))
{
};

} // namespace hi

#endif // SAFE_INVOKE_H
