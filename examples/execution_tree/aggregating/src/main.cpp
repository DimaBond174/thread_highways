#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

extern void aggregate_geo_position();

int main(int /* argc */, char ** /* argv */)
{
	aggregate_geo_position();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
