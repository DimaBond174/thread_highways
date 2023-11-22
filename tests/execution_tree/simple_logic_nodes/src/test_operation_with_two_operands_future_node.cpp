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
#include <optional>
#include <string>

namespace hi
{

TEST(TestOperationWithTwoOperandsFutureNode, StringConcatenationDirect)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};

	class NodeLogic
	{
	public:
		void operator()(std::string publication, const std::int32_t label, hi::Publisher<std::string> & publisher)
		{
			if (label == 1)
			{
				operand1 = std::move(publication);
			}
			if (label == 2)
			{
				operand2 = std::move(publication);
			}
			if (operand1.has_value() && operand2.has_value())
			{
				publisher.publish_direct(*operand1 + *operand2);
				operand1.reset();
				operand2.reset();
			}
		}

	private:
		std::optional<std::string> operand1;
		std::optional<std::string> operand2;
	};

	const auto fun =
		hi::make_self_shared<hi::DefaultNode<std::string, std::string, NodeLogic>>(hi::make_proxy(*highway), 0);
	const auto result = hi::ExecutionTreeResultNodeFabric<std::string>::create(hi::make_proxy(*highway));
	fun->connect_to_direct_channel<std::string>(result.get(), 0, 0);

	// Получаю нумерованные входящие каналы через которые можно было бы отправлять:
	auto operand1 = fun->in_param_direct_channel(1).lock();
	auto operand2 = fun->in_param_direct_channel(2).lock();

	operand2->send("the frame");
	operand1->send("Mother washed ");
	EXPECT_EQ("Mother washed the frame", result->get_result()->publication_);

	result->reset_result();
	operand1->send("12345");
	operand2->send("67890");
	EXPECT_EQ("1234567890", result->get_result()->publication_);
}

TEST(TestOperationWithTwoOperandsFutureNode, StringConcatenationOnHighway)
{
	RAIIdestroy highway{hi::make_self_shared<hi::HighWay>()};

	class NodeLogic
	{
	public:
		void operator()(std::string publication, const std::int32_t label, hi::Publisher<std::string> & publisher)
		{
			if (label == 1)
			{
				operand1 = std::move(publication);
			}
			if (label == 2)
			{
				operand2 = std::move(publication);
			}
			if (operand1.has_value() && operand2.has_value())
			{
				publisher.publish_on_highway(*operand1 + *operand2);
				operand1.reset();
				operand2.reset();
			}
		}

	private:
		std::optional<std::string> operand1;
		std::optional<std::string> operand2;
	};

	const auto fun =
		hi::make_self_shared<hi::DefaultNode<std::string, std::string, NodeLogic>>(hi::make_proxy(*highway), 0);
	const auto result = hi::ExecutionTreeResultNodeFabric<std::string>::create(hi::make_proxy(*highway));
	fun->connect_to_highway_channel<std::string>(result.get(), 0, 0);

	// Получаю нумерованные входящие каналы через которые можно было бы отправлять:
	auto operand1 = fun->in_param_highway_channel(1).lock();
	auto operand2 = fun->in_param_highway_channel(2).lock();

	operand2->send("the frame");
	operand1->send("Mother washed ");
	EXPECT_EQ("Mother washed the frame", result->get_result()->publication_);

	result->reset_result();
	operand1->send("12345");
	operand2->send("67890");
	EXPECT_EQ("1234567890", result->get_result()->publication_);
}

} // namespace hi
