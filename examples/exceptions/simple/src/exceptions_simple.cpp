#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>
#include <thread_highways/tools/exception.h>

#include <sstream>
#include <iostream>

class MakeString {
public:
    template<class T>
    MakeString& operator<< (const T& arg) {
        m_stream << arg;
        return *this;
    }
    operator std::string() const {
        return m_stream.str();
    }
protected:
    std::stringstream m_stream;
};


void f1()
{
	throw std::runtime_error("Oh, runtime_error error");
}

void f2()
{
	f1();
}

void f3()
{
	f2();
}

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
    std::cerr << std::string(level, ' ') << "exception: " << e.what() << '\n';
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

void call_and_print_exception()
{
	hi::CoutScope scope("call_and_print_exception()");

	try
	{
        f3();
	}
    catch(const hi::Exception& e)
	{
		manage_exception(e);
	}
	catch (...)
	{
        manage_exception(hi::Exception{"call_and_print_exception catch(...):", __FILE__, __LINE__, std::current_exception()});
    }
} // call_and_print_exception

int main(int /* argc */, char ** /* argv */)
{
	call_and_print_exception();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
