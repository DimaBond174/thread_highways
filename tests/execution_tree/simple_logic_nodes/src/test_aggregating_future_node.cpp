/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <map>
#include <numeric>

namespace hi
{

TEST(TestAggregatingFutureNode, AggregateSum)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};

	struct AggregatingBundle
	{
		// no need in mutex because of hi::SerialHighWay
		std::map<std::uint32_t, double> operands_map_;
	};

	const std::uint32_t expected_operands_count{10};
	auto aggregating_future = AggregatingFutureNode<double, AggregatingBundle, double>::create(
		[expected_operands_count](
			const std::uint32_t operand_id,
			const double operand_value,
			AggregatingBundle & aggregating_bundle,
			hi::IPublisher<double> & result_publisher,
			const std::uint32_t operands_count)
		{
			aggregating_bundle.operands_map_.try_emplace(operand_id, operand_value);
			if (static_cast<std::uint32_t>(aggregating_bundle.operands_map_.size()) == operands_count)
			{
				EXPECT_EQ(expected_operands_count, operands_count);
				auto summ = std::accumulate(
					aggregating_bundle.operands_map_.begin(),
					aggregating_bundle.operands_map_.end(),
					0.0,
					[](const double previous, const std::pair<const std::uint32_t, double> & p)
					{
						return previous + p.second;
					});
				result_publisher.publish(summ);
			}
		},
		highway.object_->protector_for_tests_only(),
		highway.object_);

	struct SelfProtectedChecker
	{
		SelfProtectedChecker(
			std::weak_ptr<SelfProtectedChecker> self_weak,
			const std::shared_ptr<AggregatingFutureNode<double, AggregatingBundle, double>> & node,
			IHighWayMailBoxPtr highway_mailbox)
			: future_{promise_.get_future()}
		{
			hi::subscribe(
				*node->result_channel(),
				[this](double result)
				{
					EXPECT_EQ(0, exec_counter_);
					++exec_counter_;
					promise_.set_value(result);
				},
				std::move(self_weak),
				std::move(highway_mailbox));
		}

		std::promise<double> promise_;
		std::future<double> future_;
		std::atomic<std::uint32_t> exec_counter_{0};
	};

	auto mailbox = highway.object_->mailbox();
	auto checker = hi::make_self_shared<SelfProtectedChecker>(aggregating_future, mailbox);

	// подключаем каналы операндов
	std::vector<std::shared_ptr<hi::PublishOneForMany<double>>> operands;
	for (std::uint32_t i = 0; i < expected_operands_count; ++i)
	{
		auto it = operands.emplace_back(hi::make_self_shared<hi::PublishOneForMany<double>>());
		aggregating_future->add_operand_channel(*(it->subscribe_channel()));
	}

	// шлём операнды
	double expected{0};
	double value{1.1};
	for (auto && it : operands)
	{
		it->publish(value);
		expected += value;
		value *= 2.1;
	}

	EXPECT_EQ(expected, checker->future_.get());
	EXPECT_EQ(1, checker->exec_counter_);
}

TEST(TestAggregatingFutureNode, UsingLaunchParameters)
{
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

	const std::uint32_t incoming_threads{4};
	std::atomic<std::uint32_t> unique_senders_count{0};
	auto aggregating_future = hi::AggregatingFutureNode<GeoPosition, AggregatingBundle, GeoPosition>::create(
		[&](hi::AggregatingFutureNodeLogic<GeoPosition, AggregatingBundle, GeoPosition>::LaunchParameters params)
		{
			auto && aggregating_bundle = params.aggregating_bundle_.get();
			aggregating_bundle.operands_map_[params.operand_id_] = params.operand_value_;
			double sum_latitude{0};
			double sum_longitude{0};
			for (auto && it : aggregating_bundle.operands_map_)
			{
				sum_latitude += it.second.latitude_;
				sum_longitude += it.second.longitude_;
			}
			const auto size = static_cast<std::uint32_t>(aggregating_bundle.operands_map_.size());
			unique_senders_count = size;
			params.result_publisher_.get().publish(GeoPosition{sum_latitude / size, sum_longitude / size});
		},
		highway->protector_for_tests_only(),
		highway);

	GeoPosition last_result{};
	hi::subscribe(
		aggregating_future->result_channel(),
		[&](GeoPosition result)
		{
			last_result = result;
		},
		highway->protector_for_tests_only(),
		highway->mailbox());

	const GeoPosition real_geo_position{56.7951312, 60.5928054};
	const auto location_sensor = [&](const double d_latitude, const double d_longitude)
	{
		auto sensor = hi::make_self_shared<hi::PublishOneForMany<GeoPosition>>();
		aggregating_future->add_operand_channel(sensor->subscribe_channel());
		sensor->publish(
			GeoPosition{real_geo_position.latitude_ + d_latitude, real_geo_position.longitude_ + d_longitude});
	};

	std::thread gps(
		[&]
		{
			// so that the name of the thread can be seen in the debugger
			hi::set_this_thread_name("gps");
			location_sensor(0.0, -0.1);
		});
	std::thread beidou(
		[&]
		{
			// so that the name of the thread can be seen in the debugger
			hi::set_this_thread_name("beidou");
			location_sensor(0.0, 0.1);
		});
	std::thread galileo(
		[&]
		{
			// so that the name of the thread can be seen in the debugger
			hi::set_this_thread_name("galileo");
			location_sensor(-0.1, 0.0);
		});
	std::thread glonass(
		[&]
		{
			// so that the name of the thread can be seen in the debugger
			hi::set_this_thread_name("glonass");
			location_sensor(0.1, 0.0);
		});

	gps.join();
	beidou.join();
	galileo.join();
	glonass.join();

	highway->flush_tasks();

	EXPECT_EQ(unique_senders_count, incoming_threads);
	EXPECT_DOUBLE_EQ(last_result.latitude_, real_geo_position.latitude_);
	EXPECT_DOUBLE_EQ(last_result.longitude_, real_geo_position.longitude_);

	highway->destroy();
}

} // namespace hi
