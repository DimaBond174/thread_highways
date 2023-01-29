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
    const auto highway = hi::make_self_shared<hi::HighWay>();

    struct GeoPosition
    {
      double latitude_;
      double longitude_;
      bool operator==(const GeoPosition& other)
      {
          return latitude_ == other.latitude_ && longitude_ == other.longitude_;
      }
    };

    class AggregatingBundle
    {
    public:
        std::optional<hi::LabeledPublication<GeoPosition>> operator()(hi::LabeledPublication<GeoPosition> publication)
        {
            operands_map_[publication.label_] = publication.publication_;
            // Пример игнорирования
            auto size = operands_map_.size();
            if (size < 2u) return {};

            double sum_latitude{0};
            double sum_longitude{0};
            for (auto& it: operands_map_)
            {
                sum_latitude += it.second.latitude_;
                sum_longitude += it.second.longitude_;
            }
            return GeoPosition{sum_latitude/size, sum_longitude/size};
        }
    private:
            std::map<std::int32_t, GeoPosition> operands_map_;
    };

    auto aggregating_future = hi::make_self_shared<hi::DefaultNode<GeoPosition, GeoPosition, AggregatingBundle>>(highway, 0);

    auto subscription = hi::create_subscription_for_new_only<GeoPosition>([&](GeoPosition result)
    {
       scope.print(std::string{"GeoPosition ["}.append(std::to_string(result.latitude_))
                   .append(", ").append(std::to_string(result.longitude_)).append("]"));
     });

    aggregating_future->subscribe(subscription);

   const auto location_sensor = [&](std::int32_t label, const double d_latitude, const double d_longitude)
   {
       auto sensor = hi::make_self_shared<hi::PublishOneForMany<GeoPosition>>();       
       sensor->subscribe(aggregating_future->in_param_channel(label));

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
       location_sensor(1, 0.1, -0.2);
   });
   std::thread beidou([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("beidou");
       location_sensor(2, -0.1, 0.0);
   });
   std::thread galileo([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("galileo");
       location_sensor(3, 0.2, 0.3);
   });
   std::thread glonass([&]
   {
       // so that the name of the thread can be seen in the debugger
       hi::set_this_thread_name("glonass");
       location_sensor(4, 0.1, 0.1);
   });

   std::this_thread::sleep_for(5s);

   gps.join();
   beidou.join();
   galileo.join();
   glonass.join();

   highway->flush_tasks();      
   highway->destroy();
}
