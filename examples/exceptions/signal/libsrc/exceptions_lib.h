#ifndef THREAD_HIGHWAYS_EXAMPLES_EXCEPTIONS_SIGNAL_LIBSRC_EXCEPTIONS_LIB_H
#define THREAD_HIGHWAYS_EXAMPLES_EXCEPTIONS_SIGNAL_LIBSRC_EXCEPTIONS_LIB_H

#include <thread_highways/tools/ilib.h>

extern "C" DLL_PUBLIC bool createSIGSEGV();
extern "C" DLL_PUBLIC bool createSIGABRT();

#endif // THREAD_HIGHWAYS_EXAMPLES_EXCEPTIONS_SIGNAL_LIBSRC_EXCEPTIONS_LIB_H
