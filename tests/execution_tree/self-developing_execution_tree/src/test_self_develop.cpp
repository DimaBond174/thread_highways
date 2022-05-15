/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <thread_highways/include_all.h>

#include <gtest/gtest.h>

#include <functional>

// #include <iostream>

namespace hi
{

TEST(TestSelfDevelopingExecutionTree, SelfInsertNodesIntoExecutionTree)
{
	RAIIdestroy highway{hi::make_self_shared<hi::SerialHighWay<>>()};
	auto mailbox = highway.object_->mailbox();

	ExecutionTree execution_tree{};
	auto future_result = hi::make_self_shared<hi::ResultWaitFutureNode<std::int32_t>>(mailbox);
	std::shared_ptr<hi::FutureNode<std::int32_t, std::int32_t>> pre_result_node;
	std::function<void()> insert_node = [&]()
	{
		auto future_node = hi::FutureNode<std::int32_t, std::int32_t>::create(
			[&, go_insert_new_node = true](
				std::int32_t publication,
				hi::IPublisher<std::int32_t> & result_publisher) mutable
			{
				result_publisher.publish(publication + 1);
				if (go_insert_new_node)
				{
					go_insert_new_node = false;
					insert_node();
				}
			},
			highway.object_->protector_for_tests_only(),
			highway.object_);

		if (pre_result_node)
		{
			pre_result_node->result_channel()->subscribe(future_node->subscription(false));
		}
		future_node->result_channel()->subscribe(future_result->subscription(false));
		pre_result_node = future_node;
		execution_tree.add_node(future_node);
	};

	auto execute_execution_tree = [&](const std::int32_t start_param)
	{
		std::dynamic_pointer_cast<hi::FutureNode<std::int32_t, std::int32_t>>(execution_tree.first_node())
			->subscription()
			.send(start_param);
	};

	insert_node();
	std::int32_t nodes_count{1};
	for (std::int32_t i = 0; i < 10; ++i)
	{
		future_result->reset_result();
		execute_execution_tree(i);
		EXPECT_EQ(*future_result->get_result(), (i + nodes_count));
		++nodes_count;
	}
}

/*
 Тест на возможность создания саморазвивающегося ExecutionTree
 В тесте функтор BrainNodeLogic построит цепочку NeuronNodeLogic и с помощью
 сигналов сначала будет мотивировать цепочку расти, а затем сокращаться.
 Длину цепочки можно отследить по счётчику передаваемому с сигналом BrainCommand::Calculate.

 Аналогично можно развивать ExecutionTree которое в узлах-нейронах будет реализовывать некую логику
 - это могут быть традиционные алгоритмы или обращения к специально тренированным нейросетям.
 Например это может быть конвейер по обработке изображения:
 1) нейрон ищет людей
 2) нейрон ищет у людей головы
 3) нейрон ищет на головах маску и сигнализирует в IfElseFutureNode каналы результатов
 4) самосознание нейросети адаптирует нейросеть к погодным условиям и освещению добавлением в
  цепочку обработки узлов с фильтрами изображения, пробует тренировать свои нейросети, самообучаться
  для улучшения качества распознавания.
  Сочетание узлов с традиционными алгоритмами и узлов с нейросетями позволит решать более сложные задачи
  энергоэффективнее.
*/
TEST(TestSelfDevelopingExecutionTree, SelfInsertAndRemoveNodesInExecutionTree)
{
	struct BrainCommand
	{
		enum class Command : std::uint16_t
		{
			ConstructNeurons = 0,
			RemoveNeurons = 1,
			Destroy = 2,
			NodeDestroy = 3,
			PrevNode = 4,
			NextNode = 5,
			YourNode = 6,
			Calculate = 7,
			AddNode = 8,
			RemoveNode = 9
		};

		const Command command_;
		const std::shared_ptr<void> data_;
	};

	struct ResultOfReflection
	{
		enum class ResultType
		{
			NeuralNetworkExpanded,
			Destroyed,
			NeuralNetworkShrunked
		};

		const ResultType result_type_;
		const std::shared_ptr<void> result_;
	};

	using NeuronNode = hi::FutureNode<std::shared_ptr<BrainCommand>, std::shared_ptr<BrainCommand>>;
	using NeuronNodePtr = std::shared_ptr<NeuronNode>;
	using BrainNode = hi::IfElseFutureNode<
		std::shared_ptr<BrainCommand>,
		std::shared_ptr<ResultOfReflection>,
		std::shared_ptr<BrainCommand>>;

	struct NeuronNodeLogic
	{

		void operator()(
			std::shared_ptr<BrainCommand> command,
			hi::IPublisher<std::shared_ptr<BrainCommand>> & result_publisher)
		{
			switch (command->command_)
			{
			case BrainCommand::Command::NextNode:
				{
					next_ = std::static_pointer_cast<NeuronNode>(command->data_);
					break;
				}
			case BrainCommand::Command::PrevNode:
				{
					prev_ = std::static_pointer_cast<NeuronNode>(command->data_);
					break;
				}
			case BrainCommand::Command::YourNode:
				{
					self_ = std::static_pointer_cast<NeuronNode>(command->data_);
					if (self_ && prev_)
					{
						protector_ = std::make_shared<bool>();
						prev_->result_channel()->subscribe(
							hi::protect(self_->subscription(false), std::weak_ptr(protector_)));
					}
					break;
				}
			case BrainCommand::Command::NodeDestroy:
				{
					self_.reset();
					prev_.reset();
					protector_.reset();
					// next_ оставляем - односвязанный список деаллоцируется когда BrainNodeLogic удалит first_neuron_
					result_publisher.publish(std::move(command));
					break;
				}
			case BrainCommand::Command::AddNode:
				{
					if (!prev_)
					{
						result_publisher.publish(std::move(command));
						break;
					}
					// Главный мозг прислал узел с логикой, которую необходимо встроить	в цепочку размышлений
					auto new_neuron = std::static_pointer_cast<NeuronNode>(command->data_);
					EXPECT_TRUE(!!new_neuron);
					// Встраивать будем между собой и предыдущим нейроном.
					// Убираем свою подписку к предыдущему нейрону.
					protector_ = std::make_shared<bool>();
					// Следующему даём команду подписаться к предыдущему
					new_neuron->subscription(false).send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::PrevNode, prev_}));
					new_neuron->subscription(false).send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::NextNode, self_}));
					new_neuron->subscription(false).send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::YourNode, new_neuron}));
					new_neuron->result_channel()->subscribe(
						hi::protect(self_->subscription(false), std::weak_ptr(protector_)));
					prev_ = new_neuron;
					break;
				}
			case BrainCommand::Command::RemoveNode:
				{
					if (!next_ || !prev_)
					{
						result_publisher.publish(std::move(command));
						break;
					}
					// Убираем свою подписку к предыдущему
					protector_.reset();
					// Следующему даём команду подписаться к предыдущему
					next_->subscription(false).send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::PrevNode, prev_}));
					next_->subscription(false).send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::YourNode, next_}));
					// Даём возможность себя деаллоцировать
					self_.reset();
					prev_.reset();
					break;
				}
			case BrainCommand::Command::Calculate:
				{
					auto data = std::static_pointer_cast<std::int32_t>(command->data_);
					EXPECT_TRUE(data);
					++(*data);
					result_publisher.publish(std::move(command));
					break;
				}
			default:
				{
					EXPECT_TRUE(false);
					break;
				}
			} // switch
		}

		NeuronNodePtr self_{nullptr};
		NeuronNodePtr prev_{nullptr};
		NeuronNodePtr next_{nullptr};
		std::shared_ptr<bool> protector_;
	};

	struct BrainNodeLogic
	{
		BrainNodeLogic(hi::IHighWayPtr highway)
			: highway_{std::move(highway)}
		{
		}

		void operator()(
			std::shared_ptr<BrainCommand> command,
			IPublisher<std::shared_ptr<ResultOfReflection>> & if_result_publisher,
			IPublisher<std::shared_ptr<BrainCommand>> & else_result_publisher)
		{
			switch (command->command_)
			{
			case BrainCommand::Command::ConstructNeurons:
				{
					auto self = std::static_pointer_cast<BrainNode>(command->data_);
					EXPECT_TRUE(!!self);
					auto first = NeuronNode::create(NeuronNodeLogic{}, highway_->protector_for_tests_only(), highway_);
					auto last = NeuronNode::create(NeuronNodeLogic{}, highway_->protector_for_tests_only(), highway_);
					self->else_result_channel()->subscribe(first->subscription(false));
					last->result_channel()->subscribe(self->subscription(false));
					auto first_subscription = first->subscription(false);
					auto last_subscription = last->subscription(false);
					first_subscription.send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::NextNode, last}));
					first_subscription.send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::YourNode, first}));
					last_subscription.send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::PrevNode, first}));
					last_subscription.send(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::YourNode, last}));
					first_subscription.send(std::make_shared<BrainCommand>(
						BrainCommand{BrainCommand::Command::Calculate, std::make_shared<std::int32_t>(0)}));
					// Чтобы цепочка shared_ptr -ов не удалялась:
					first_neuron_ = first;
					break;
				}
			case BrainCommand::Command::Calculate:
				{
					auto data = std::static_pointer_cast<std::int32_t>(command->data_);
					EXPECT_TRUE(!!data);
					// std::cout << "brain data=" << *data << std::endl;
					if (go_grow_)
					{
						if (*data < 100)
						{
							auto node =
								NeuronNode::create(NeuronNodeLogic{}, highway_->protector_for_tests_only(), highway_);
							else_result_publisher.publish(std::make_shared<BrainCommand>(
								BrainCommand{BrainCommand::Command::AddNode, std::move(node)}));
							else_result_publisher.publish(std::make_shared<BrainCommand>(
								BrainCommand{BrainCommand::Command::Calculate, std::make_shared<std::int32_t>(0)}));
						}
						else
						{
							if_result_publisher.publish(std::make_shared<ResultOfReflection>(
								ResultOfReflection{ResultOfReflection::ResultType::NeuralNetworkExpanded, nullptr}));
						}
					}
					else
					{
						if (*data > 10)
						{
							else_result_publisher.publish(std::make_shared<BrainCommand>(
								BrainCommand{BrainCommand::Command::RemoveNode, nullptr}));
							else_result_publisher.publish(std::make_shared<BrainCommand>(
								BrainCommand{BrainCommand::Command::Calculate, std::make_shared<std::int32_t>(0)}));
						}
						else
						{
							if_result_publisher.publish(std::make_shared<ResultOfReflection>(
								ResultOfReflection{ResultOfReflection::ResultType::NeuralNetworkShrunked, nullptr}));
						}
					}
					break;
				}
			case BrainCommand::Command::RemoveNeurons:
				{
					go_grow_ = false;
					else_result_publisher.publish(std::make_shared<BrainCommand>(
						BrainCommand{BrainCommand::Command::Calculate, std::make_shared<std::int32_t>(0)}));
					break;
				}
			case BrainCommand::Command::Destroy:
				{
					else_result_publisher.publish(
						std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::NodeDestroy, nullptr}));
					break;
				}
			case BrainCommand::Command::NodeDestroy:
				{
					first_neuron_.reset();
					if_result_publisher.publish(std::make_shared<ResultOfReflection>(
						ResultOfReflection{ResultOfReflection::ResultType::Destroyed, nullptr}));
					break;
				}
			default:
				{
					EXPECT_TRUE(false);
					break;
				}
			}
		}
		bool go_grow_{true};
		INodePtr first_neuron_{nullptr};
		hi::IHighWayPtr highway_;
	};

	RAIIdestroy brain_highway{hi::make_self_shared<hi::SerialHighWay<>>()};
	auto brain_highway_mailbox = brain_highway.object_->mailbox();
	auto future_result =
		hi::make_self_shared<hi::ResultWaitFutureNode<std::shared_ptr<ResultOfReflection>>>(brain_highway_mailbox);
	auto brain_node = BrainNode::create(
		BrainNodeLogic{brain_highway.object_},
		brain_highway.object_->protector_for_tests_only(),
		brain_highway.object_);
	brain_node->if_result_channel()->subscribe(future_result->subscription(false));

	// Начало мыслительного процесса
	brain_node->subscription().send(
		std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::ConstructNeurons, brain_node}));
	auto result = future_result->get_result();
	EXPECT_EQ((*result)->result_type_, ResultOfReflection::ResultType::NeuralNetworkExpanded);

	// Сокращение мыслительного процесса
	future_result->reset_result();
	brain_node->subscription().send(
		std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::RemoveNeurons, brain_node}));
	result = future_result->get_result();
	EXPECT_EQ((*result)->result_type_, ResultOfReflection::ResultType::NeuralNetworkShrunked);

	// Разрушение мыслительного процесса
	future_result->reset_result();
	brain_node->subscription().send(
		std::make_shared<BrainCommand>(BrainCommand{BrainCommand::Command::Destroy, brain_node}));
	result = future_result->get_result();
	EXPECT_EQ((*result)->result_type_, ResultOfReflection::ResultType::Destroyed);
}

} // namespace hi
