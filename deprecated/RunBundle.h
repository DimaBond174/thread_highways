#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_RUNBUNDLE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_RUNBUNDLE_H

#include <atomic>
#include <cstdint>
#include <functional>

namespace hi
{

// Однажды создаётся в хайвее и в запускаемые таски прокидывается как const &
struct RunBundle
{
	// identifier with which this highway works now
	const std::reference_wrapper<const std::atomic<std::uint32_t>> global_run_id_;
	// your_run_id - identifier with which this highway was running when this task started
	const std::uint32_t your_run_id_;
};

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_RUNBUNDLE_H
