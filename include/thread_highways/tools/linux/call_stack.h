#ifndef THREADS_HIGHWAYS_TOOLS_LINUX_CALL_STACK_H
#define THREADS_HIGHWAYS_TOOLS_LINUX_CALL_STACK_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <thread_highways/tools/common/call_stack_struct.h>
#include <thread_highways/tools/linux/os_tools.h>

#include <execinfo.h>
#include <link.h>
#include <stdlib.h>

#include <cstdint>
#include <regex>
#include <string>

#include <iostream>

namespace hi {
namespace  {

thread_local std::vector<StackBinary> binaries;

static int dl_iterate_phdr_callback (struct dl_phdr_info *info, size_t , void *)
{
    binaries.emplace_back(StackBinary{std::string{info->dlpi_name},
         static_cast<std::uint64_t>(info->dlpi_addr)});
    return 0;
}

} // namespace

inline std::vector<StackBinary> get_loaded_modules()
{
    binaries.clear();
    dl_iterate_phdr(dl_iterate_phdr_callback, nullptr);
    return binaries;
} // get_loaded_modules

inline std::vector<StackFrame> get_stack_frames()
{
    const std::size_t stackSize{100};
    std::vector<void *>stackBuf(stackSize, nullptr);
    int nptrs{0};
    while(true)
    {
        nptrs = backtrace(stackBuf.data(), static_cast<int>(stackBuf.size()));
        if (nptrs <= 0) return {};
        if (static_cast<std::size_t>(nptrs) < stackBuf.size()) break;
        stackBuf.resize(stackBuf.size() + stackSize);
    }

    char **strings = backtrace_symbols(stackBuf.data(), nptrs);
    if (!strings) return {};
    std::vector<StackFrame> callStack;
    callStack.reserve(nptrs);

    for(int i = 0; i < nptrs; ++i)
    {
        callStack.emplace_back(
            StackFrame{
                        std::string{strings[i]},
                        reinterpret_cast<std::uint64_t>(stackBuf[i])
            });
    }
    std::free(strings);
    return callStack;
}

inline CallStack get_call_stack()
{
    return CallStack{get_loaded_modules(), get_stack_frames()};
}

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_LINUX_CALL_STACK_H
