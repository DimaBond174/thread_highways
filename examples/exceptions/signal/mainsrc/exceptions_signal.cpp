#include "exceptions_lib.h"

#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>
#include <thread_highways/tools/exception.h>

#include <signal.h>

#include <sstream>
#include <iostream>

void print_exception(const hi::Exception& e, int level =  0);
void print_exception(const std::exception& e, int level =  0);
void print_exception(const hi::Exception& e, int level)
{
    std::cerr << std::string(level, ' ') << "exception: " << e.what_as_string() << std::endl;
    std::cerr << std::string(level, ' ') << e.call_stack() << std::endl;

    try {
        std::rethrow_if_nested(e);
    } catch(const hi::Exception& nestedException) {
        print_exception(nestedException, level+1);
    } catch(const std::exception& nestedException) {
        print_exception(nestedException, level+1);
    } catch(...) {}
}

void print_exception(const std::exception& e, int level)
{
    std::cerr << "\n============================";
    std::cerr << std::string(level, ' ') << "exception lvl" << level << ": " << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch(const hi::Exception& nestedException) {
        print_exception(nestedException, level+1);
    } catch(const std::exception& nestedException) {
        print_exception(nestedException, level+1);
    } catch(...) {}
}

void manage_exception(const hi::Exception& e)
{
	print_exception(e);
}

//void signal_handler(int sig, struct sigcontext ctx)
void signal_handler(int sig)
{
  switch (sig)
  {
  case SIGSEGV:
      manage_exception(hi::Exception{"SIGSEGV catch(...):", __FILE__, __LINE__,  std::current_exception()});
  break;
  case SIGABRT:
    manage_exception(hi::Exception{"SIGABRT catch(...):", __FILE__, __LINE__,  std::current_exception()});
  break;
  default:
    manage_exception(hi::Exception{"unknown error catch(...):", __FILE__, __LINE__,  std::current_exception()});
  break;
  }

  exit(0);
}

void setup_signal_handler()
{
  /* Install our signal handler */
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  /* ... add any other signal here */
}

int main(int /* argc */, char ** /* argv */)
{
    setup_signal_handler();

    //createSIGABRT();
    createSIGSEGV();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
