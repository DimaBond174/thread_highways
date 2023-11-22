#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

extern void aggregate_geo_position();
extern void wifi_beacon_frames_parser();

int main(int /* argc */, char ** /* argv */)
{
	aggregate_geo_position();
	wifi_beacon_frames_parser();

	std::cout << "Tests finished" << std::endl;
	return 0;
}
