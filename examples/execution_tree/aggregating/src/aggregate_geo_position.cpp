#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <map>
#include <numeric>

using namespace std::chrono_literals;


/*
* An example of aggregating geodata from various sources
* to obtain a more accurate geolocation.
*/
void aggregate_geo_position()
{
    hi::CoutScope scope("aggregate_geo_position()");
    const auto highway = hi::make_self_shared<hi::SerialHighWay<>>();
    const auto highway_mailbox = highway->mailbox();

    struct GeoPosition
    {
      double latitude_;
      double longitude_;
    };

    struct AggregatingBundle
    {
            // no need in mutex because of hi::SerialHighWay
            std::map<std::uint32_t, GeoPosition> operands_map_;
    };

    auto aggregating_future = hi::AggregatingFutureNode<GeoPosition, AggregatingBundle, GeoPosition>::create(
            [&](hi::AggregatingFutureNodeLogic<GeoPosition, AggregatingBundle, GeoPosition>::LaunchParameters params)
            {
                auto &&aggregating_bundle = params.aggregating_bundle_.get();
                scope.print(std::string{"params.operand_id_ = "}.append(std::to_string(params.operand_id_)));
                scope.print(std::string{"aggregating_bundle.operands_map_.size = "}.append(std::to_string(aggregating_bundle.operands_map_.size())));
                aggregating_bundle.operands_map_[params.operand_id_] = params.operand_value_;
                double sum_latitude{0};
                double sum_longitude{0};
                for (auto && it: aggregating_bundle.operands_map_)
                {
                    sum_latitude += it.second.latitude_;
                    sum_longitude += it.second.longitude_;
                }
                const auto size = aggregating_bundle.operands_map_.size();
                scope.print(std::string{"aggregating_bundle.operands_map_.size = "}.append(std::to_string(size)));
                params.result_publisher_.get().publish(GeoPosition{sum_latitude/size, sum_longitude/size});
            },
            highway->protector_for_tests_only(),
            highway);

   hi::subscribe(aggregating_future->result_channel(),
       [&](GeoPosition result)
    {
       scope.print(std::string{"GeoPosition ["}.append(std::to_string(result.latitude_))
                   .append(", ").append(std::to_string(result.longitude_)).append("]"));
     }, highway->protector_for_tests_only(), highway->mailbox());

   const auto location_sensor = [&](const double d_latitude, const double d_longitude)
   {
       auto sensor = hi::make_self_shared<hi::PublishOneForMany<GeoPosition>>();
       aggregating_future->add_operand_channel(sensor->subscribe_channel());
       GeoPosition geo_position{56.7951312, 60.5928054};
       const auto start_time = std::chrono::steady_clock::now();
       for (auto cur_time = std::chrono::steady_clock::now(); (cur_time - start_time) < 5s; cur_time = std::chrono::steady_clock::now())
       {
           std::this_thread::sleep_for(100ms);
           geo_position.latitude_ += d_latitude;
           geo_position.longitude_ += d_longitude;
           sensor->publish(geo_position);
        }
   };

   std::thread gps([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("gps");
       location_sensor(0.1, -0.2);
   });
   std::thread beidou([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("beidou");
       location_sensor(-0.1, 0.0);
   });
   std::thread galileo([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("galileo");
       location_sensor(0.2, 0.3);
   });
   std::thread glonass([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("glonass");
       location_sensor(0.1, 0.1);
   });

   std::this_thread::sleep_for(5s);

   gps.join();
   beidou.join();
   galileo.join();
   glonass.join();

   highway->flush_tasks();      
   highway->destroy();
}
