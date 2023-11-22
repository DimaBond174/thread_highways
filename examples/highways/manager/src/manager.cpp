#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <stack>
#include <unordered_map>

using namespace std::chrono_literals;

template <typename T>
std::string to_string(std::string title, const T & vec)
{
	title.append(": ");
	for (auto it : vec)
	{
		title.append(std::to_string(it)).append(", ");
	}
	return title;
}

/*
	Размещение многопоточных задач через HighWaysManager.
	Хайвеи из под капота HighWaysManager переиспользуются также в подписках.

	Тестовая задача заключается в распараллеливании расчётов объекта Data.
	Задача выполнена если каждый объект Data прошел loop_cnt циклов распараллеливания.
*/
void parallel_calculations(
	const std::uint32_t loop_cnt,
	const std::int32_t threads_cnt,
	std::shared_ptr<hi::HighWaysManager> highways_manager)
{
	hi::CoutScope scope(
		"parallel_calculations() for loop_cnt=" + std::to_string(loop_cnt)
		+ ", threads_cnt=" + std::to_string(threads_cnt));

	struct Data
	{
		Data(std::int32_t i)
			: data_name_{"data_name_" + std::to_string(i)}
			, result_future_{result_.get_future()} {};

		std::string data_name_;
		std::uint32_t data_; // текущий результат
		std::vector<std::int32_t> channels_path_; // каналы из которых приходили данные
		std::vector<std::uint32_t> data_path_; // какие данные приходили из каналов
		std::uint32_t loop_num_{0}; // номер прохода по нейросети
		std::promise<std::uint32_t> result_; // канал публикации результата
		std::future<std::uint32_t> result_future_;
	};
	using DataPtr = std::shared_ptr<Data>;

	struct CalculationsResult
	{
		DataPtr data_;
		std::uint32_t result_; // результаты сложных вычислений
	};

	class AggregatingBundle
	{
	public:
		AggregatingBundle(std::uint32_t loop_cnt, std::int32_t threads_cnt, hi::CoutScope & scope)
			: loop_cnt_{loop_cnt}
			, threads_cnt_{threads_cnt}
			, scope_{scope}
		{
		}

		void operator()(
			CalculationsResult сalculations_result,
			const std::int32_t label,
			hi::Publisher<CalculationsResult> & publisher)
		{
			scope_.print(
				сalculations_result.data_->data_name_ + " on channel " + std::to_string(label)
				+ " received сalculations_result=" + std::to_string(сalculations_result.result_));
			auto & cnt = operands_map_[сalculations_result.data_];
			cnt += 1;
			сalculations_result.data_->data_ += сalculations_result.result_;
			сalculations_result.data_->channels_path_.emplace_back(label);
			сalculations_result.data_->data_path_.emplace_back(сalculations_result.result_);
			if (cnt == threads_cnt_)
			{
				cnt = 0;
				сalculations_result.data_->loop_num_ += 1u;
				if (loop_cnt_ > сalculations_result.data_->loop_num_)
				{
					сalculations_result.result_ = сalculations_result.data_->data_;
					publisher.publish_on_highway(сalculations_result);
				}
				else
				{
					сalculations_result.data_->result_.set_value(сalculations_result.data_->data_);
				}
			}
		}

	private:
		const std::uint32_t loop_cnt_;
		const std::int32_t threads_cnt_;
		std::unordered_map<DataPtr, std::int32_t> operands_map_;
		hi::CoutScope & scope_;
	};

	// auto aggregating_node = hi::make_self_shared<hi::DefaultNode<CalculationsResult, DataPtr,
	// AggregatingBundle>>(highways_manager->get_highway(100), 0);
	auto aggregating_node = hi::ExecutionTreeDefaultNodeFabric<CalculationsResult, CalculationsResult>::create(
		AggregatingBundle{loop_cnt, threads_cnt, scope},
		highways_manager->get_highway(100));

	std::vector<std::shared_ptr<hi::ISubscription<CalculationsResult>>> incoming_channels;
	// Создаю каналы через которые будут приходить данные.
	// Можно обойтись и одним каналом если нет необходимости знать идентификатор источника входящих данных.
	for (std::int32_t i = 0; i < threads_cnt; ++i)
	{
		incoming_channels.emplace_back(aggregating_node->in_param_highway_channel(i, false).lock());
	}

	auto execute_parallel = [&](CalculationsResult data)
	{
		for (std::int32_t i = 0; i < threads_cnt; ++i)
		{
			highways_manager->execute(
				[&, base_data = data.result_, i, data]
				{
					std::this_thread::sleep_for(100ms); // сложные вычисления
					auto result = base_data + i;
					scope.print(data.data_->data_name_ + ": result=" + std::to_string(result));
					incoming_channels[i]->send(CalculationsResult{data.data_, result});
				});
		}
	};

	// Зацикливаю в круг: результат aggregating_node запустит новые параллельные вычисления
	auto subscription = hi::create_subscription<CalculationsResult>(
		[&](CalculationsResult result)
		{
			execute_parallel(std::move(result));
		});
	aggregating_node->subscribe_direct(subscription);

	// Вхожу в цикл с данными:
	std::vector<DataPtr> datas;
	for (std::int32_t j = 0; j < 3; ++j)
	{
		auto data = std::make_shared<Data>(j);
		datas.emplace_back(data);
		for (std::int32_t i = 0; i < threads_cnt; ++i)
		{
			incoming_channels[i]->send(CalculationsResult{data, 0});
		}
	}

	// Встаю на ожидание результатов:
	for (auto it : datas)
	{
		auto res = it->result_future_.get();
		std::string str{"Result ready for " + it->data_name_ + "=" + std::to_string(res)};
		str.append(to_string("\nchannels_path_", it->channels_path_)).append(to_string("\ndata_path_", it->data_path_));
		scope.print(str);
	}
} // parallel_calculations()

void load_balancing(const std::int32_t cnt, std::shared_ptr<hi::HighWaysManager> highways_manager)
{
	hi::CoutScope scope("load_balancing() for cnt=" + std::to_string(cnt));
	std::atomic<std::int32_t> executed{cnt};
	std::stack<hi::HighWayProxyPtr> highways;
	for (std::int32_t i = 0; i < cnt; ++i)
	{
		auto highway = highways_manager->get_highway(45);
		highways.emplace(highway);
		scope.print("highways_manager.size()=" + std::to_string(highways_manager->size()));

		highway->execute(
			[&]()
			{
				std::this_thread::sleep_for(100ms);
				--executed;
				scope.print("highways_manager.size()=" + std::to_string(highways_manager->size()));
			});
	}

	while (executed > 0)
	{
		std::this_thread::sleep_for(100ms);
	}

	while (!highways.empty())
	{
		highways.pop();
		scope.print("highways_manager.size()=" + std::to_string(highways_manager->size()));
	}
}

int main(int /* argc */, char ** /* argv */)
{
	// Хайвеи без контроля времени исполнения тасков
	parallel_calculations(10, 5, hi::make_self_shared<hi::HighWaysManager>(1u, 2u));

	// Хайвеи с контролем времени исполнения тасков
	parallel_calculations(
		10,
		5,
		hi::make_self_shared<hi::HighWaysManager>(
			1u,
			1u,
			false,
			[](const hi::Exception & ex)
			{
				throw ex;
			},
			"HighWaysManager",
			65000u,
			hi::HighWaysManager::HighWaySettings{std::chrono::milliseconds{1000}, 65000u}));

	// Проверка выдачи хайвеев без авторегулирования
	load_balancing(10, hi::make_self_shared<hi::HighWaysManager>(1u, 1u, false));

	// Проверка выдачи хайвеев c авторегулированием
	load_balancing(10, hi::make_self_shared<hi::HighWaysManager>(1u, 1u, true));

	std::cout << "Tests finished" << std::endl;
	return 0;
}
