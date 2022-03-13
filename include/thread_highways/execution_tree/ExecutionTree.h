#ifndef EXECUTIONTREE_H
#define EXECUTIONTREE_H

#include <thread_highways/channels/PublishManyForMany.h>
#include <thread_highways/execution_tree/CurrentExecutedNode.h>
#include <thread_highways/execution_tree/INode.h>
#include <thread_highways/tools/make_self_shared.h>

#include <deque>
#include <memory>

namespace hi
{

/*
Блок схема (дерево исполнения).
Просто хранит shared_ptr -ы узлов дерева.
Между собой узлы связаны через weak_ptr, поэтому нужно внешнее хранилище.
Удалил ExecutionTree == остановил исполнение блок схемы

Самой блок схемы тут нет - она реализуется через подписки между узлами
на результаты узлов и анализаторов результатов узлов.

Выводом финального результата(ов) занимаются сами узлы.

Блок схема может быть многоразовой, одноразовой, зацикленной.
Логика блок схемы, в том числе самоликвидация, в узлах блок схемы.

Например: результирующий узел блок-схемы может хранить саму блок-схему
и удалять её при необходимости.

Например2: циклические операции
 (например регулярное обновление пробочной информации на карте)
 может шедулить первый запускающий процесс узел блок схемы.

*/
class ExecutionTree
{
public:
	ExecutionTree()
		: current_executed_node_publisher_{make_self_shared<PublishManyForMany<CurrentExecutedNode>>()}
	{
	}

	~ExecutionTree()
	{
		destroy();
	}

	void destroy()
	{
		tree_.clear();
		current_executed_node_publisher_.reset();
	}

	std::shared_ptr<PublishManyForMany<CurrentExecutedNode>> current_executed_node_publisher()
	{
		return current_executed_node_publisher_;
	}

	void add_node(INodePtr node)
	{
		tree_.push_back(std::move(node));
	}

private:
	// todo канал в который узлы будут слать кто запустился
	// сделать в Android демку где прокидывать Java узлы в дерево и
	// анимировать ход исполнения -какие узлы запустились , какие остановились
	// т.е. канал видимо со структурой из номера узла и состоянием
	std::shared_ptr<PublishManyForMany<CurrentExecutedNode>> current_executed_node_publisher_;

	std::deque<INodePtr> tree_;
};

using ExecutionTreePtr = std::shared_ptr<ExecutionTree>;

} // namespace hi

#endif // EXECUTIONTREE_H
