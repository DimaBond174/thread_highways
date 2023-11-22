#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <cmath>
#include <set>
#include <vector>

/*
 * An example of parsing the BIG data (transmission without copying).
 */
void wifi_beacon_frames_parser()
{
	hi::CoutScope scope("wifi_beacon_frames_parser()");
	const auto highway = hi::make_self_shared<hi::HighWay>();

	struct BigData
	{
		BigData(hi::CoutScope & printer)
			: printer_{printer}
		{
			printer_.print("BigData can't copy or move");
		}

		BigData(const BigData & other)
			: printer_{other.printer_}
		{
			assert(false);
		}

		BigData(BigData && other)
			: printer_{other.printer_}
		{
			assert(false);
		}

		~BigData()
		{
			printer_.print("~BigData");
		}

		bool operator==(const BigData &)
		{
			assert(false);
			return false;
		}

		hi::CoutScope & printer_;
		std::vector<std::vector<std::uint8_t>> beacon_frames_;
	};

	struct ParsedBeaconData
	{
		std::string ssid;
	};

	class BeaconsParser
	{
	public:
		void operator()(
			std::shared_ptr<BigData> publication,
			const std::int32_t /* label */,
			hi::Publisher<ParsedBeaconData> & publisher)
		{
			for (const auto & it : publication->beacon_frames_)
			{
				if (it.size() < 3)
					continue;
				switch (it[0])
				{
				case 0:
					{ // SSID
						const char * begin = reinterpret_cast<const char *>(&it[2]);
						auto ssid = std::string{begin, it[1]};
						auto found = ssids_.find(ssid);
						if (found == ssids_.end())
						{
							publisher.publish_direct(ParsedBeaconData{ssid});
							ssids_.emplace(std::move(ssid));
						}
						break;
					}
				default:
					break;
				}
			}
		}

	private:
		std::set<std::string> ssids_;
	};

	// Кто будет поставлять сырые данные
	auto wifi = hi::make_self_shared<hi::PublishOneForMany<std::shared_ptr<BigData>>>();

	// Кто будет анализировать сырые данные
	auto parser =
		hi::make_self_shared<hi::DefaultNode<std::shared_ptr<BigData>, ParsedBeaconData, BeaconsParser>>(highway, 0);

	// Кто будет потреблять готовые данные
	auto subscription = hi::create_subscription<ParsedBeaconData>(
		[&](ParsedBeaconData result)
		{
			scope.print(std::string{"ParsedBeaconData.SSID: "} + result.ssid);
		});

	// Блок схема
	parser->subscribe_direct(subscription);
	wifi->subscribe(parser->in_param_direct_channel());

	// Подписки приходят в потоке highway, убедимся что подписки встали
	highway->flush_tasks();

	// Исходные точки в воздухе:
	auto big_data = std::make_shared<BigData>(scope);
	for (int i = 1; i < 10; ++i)
	{
		const std::string ssid{std::to_string(std::pow(2, i))};
		std::vector<std::uint8_t> beacon(ssid.size() + 2, 0);
		beacon[1] = static_cast<std::uint8_t>(ssid.size());
		memcpy(&beacon[2], ssid.data(), ssid.size());
		big_data->beacon_frames_.emplace_back(std::move(beacon));
	}
	wifi->publish(big_data);

	// Имитируем появление  новых WiFi точек - должны приходить только новые данные
	for (int i = 10; i < 20; ++i)
	{
		const std::string ssid{std::to_string(std::pow(2, i))};
		std::vector<std::uint8_t> beacon(ssid.size() + 2, 0);
		beacon[1] = static_cast<std::uint8_t>(ssid.size());
		memcpy(&beacon[2], ssid.data(), ssid.size());
		big_data->beacon_frames_.emplace_back(std::move(beacon));
		scope.print("======= New BigData ======");
		wifi->publish(big_data);
		highway->flush_tasks();
	}

	highway->flush_tasks();
	highway->destroy();
}
