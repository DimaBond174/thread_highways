#if _WIN32
#	include <thread_highways/tools/os/windows.h>
#elif __APPLE__
#	include <thread_highways/tools/os/apple.h>
#elif __ANDROID__
#	include <thread_highways/tools/os/android.h>
#elif __linux__
#	include <thread_highways/tools/os/linux.h>
#endif