#ifndef THREADS_HIGHWAYS_TOOLS_RESULT_CODE_H
#define THREADS_HIGHWAYS_TOOLS_RESULT_CODE_H

#include <cstdint>

namespace hi
{

using result_t = std::int32_t;

// "Not run yet" code (like empty optional)
constexpr result_t noResult{0};

// Success codes > 0
constexpr result_t ok{1};
constexpr result_t okCreatedNew{2};
constexpr result_t okReplaced{3};
constexpr result_t okReady{4};
constexpr result_t okInProcess{5};

// Error codes < 0
constexpr result_t eFail{-1};
constexpr result_t eNoMemory{-2};
constexpr result_t eFailMoreThanIHave{-3};

} // namespace hi

#define RETURN_CODE_IF_FAIL(val) \
	if (auto code = (val); code < 0) \
	return code

#endif // THREADS_HIGHWAYS_TOOLS_RESULT_CODE_H
