/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H
#define THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H

#include <thread_highways/channels/highway_sticky_publisher.h>
#include <thread_highways/channels/i_subscription.h>
#include <thread_highways/execution_tree/node_progress.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace hi
{

// Входящие параметры и исходящие результаты промечаются label
// Например в логическом ветвлении результата на if(true == label{1}) или else(false == label{0})
// Например входящие операнды арифметического выражения x[label{0}] + x[label{1}] / x[label{3}]
// Метка label позволяет смаршрутизировать входящий параметр и исходящий результат по блок схеме алгоритма
template <typename Publication>
struct LabeledPublication
{
	LabeledPublication(Publication publication, std::int32_t label = {})
		: publication_{std::move(publication)}
		, label_{label}
	{
	}

	Publication publication_;
	std::int32_t label_;
};

template <typename Publication>
struct LabeledWeakSubscription
{
	LabeledWeakSubscription(std::weak_ptr<ISubscription<Publication>> subscription, std::int32_t label = {})
		: subscription_{std::move(subscription)}
		, label_{label}
	{
	}

	const std::weak_ptr<ISubscription<Publication>> subscription_;
	const std::int32_t label_;
	LabeledWeakSubscription * next_in_stack_{nullptr};
};

template <typename Publication>
struct LabeledStrongSubscription
{
	LabeledStrongSubscription(std::shared_ptr<ISubscription<Publication>> subscription, std::int32_t label = {})
		: subscription_{std::move(subscription)}
		, label_{label}
	{
	}

	const std::shared_ptr<ISubscription<Publication>> subscription_;
	const std::int32_t label_;
	LabeledStrongSubscription * next_in_stack_{nullptr};
};

template <typename InParam>
class InParamChannel
{
public:
	// Получить входящий канал на определённый операнд
	// Например: (x1 + x2 * x3) / x4
	// - можно получить канал на каждый x[N] и слать на вход операции операнд сразу в нужное место
	// Важно: подписка работает пока хранится shared_ptr, поэтому
	// std::shared_ptr<ISubscription<LabeledVar<T>>> subscription необходимо хранить у себя
	// (например в std::map<std::int32_t, shared_> или иным способом если например label{x} используется
	// для подписки на результаты нескольких источников
	// Подписка с перепланированием на highway
	virtual std::weak_ptr<ISubscription<InParam>> in_param_highway_channel(
		std::int32_t label = {},
		bool send_may_fail = true) = 0;

	// Подписка получает сообщения в потоке Паблишера, нет копирования публикации
	// node_logic отвечает за защиту в многопоточке
	// ВНИМАНИЕ: используйте этот канал если уверены что не будет гонок или переполнения стека на цикле вызовов
	virtual std::weak_ptr<ISubscription<InParam>> in_param_direct_channel(std::int32_t label = {}) = 0;

	// Для удобства сразу возможность отправить без подписки в потоке highway
	virtual void send_param_in_highway_channel(InParam param) = 0;

	// Для удобства сразу возможность отправить без подписки напрямую
	// ВНИМАНИЕ: используйте этот канал если уверены что не будет гонок или переполнения стека на цикле вызовов
	virtual void send_param_in_direct_channel(const InParam & param) = 0;
};

template <typename Publication>
class Publisher
{
public:
	virtual ~Publisher() = default;

	// Прогресс или код ошибки
	virtual void progress(std::int32_t achieved_progress) = 0;

	// Безопасная публикация с перепланированием отправки в потоке встроенного highway
	virtual void publish_on_highway(LabeledPublication<Publication> publication) = 0;

	// Быстрая публикация без перепланирования на поток highway
	// ВНИМАНИЕ!: необходимо быть уверенным что в момент отправки не появится новых подписчиков на результаты узла
	// (в момент отправки удаляются протухшие подписки и может быть гонка между добавлением и удалением подписчиков)
	// Добавление новых подписчиков всегда проходит в потоке highway,
	// поэтому если узел обрабатывает свои входящие каналы в потоке highway ( == in_param_highway_channel)
	// то лучше (и это безопасно) всегда публиковать через publish_direct
	virtual void publish_direct(LabeledPublication<Publication> publication) = 0;

	// Подписывает на результат с определённой логической ветки
	// Например в узле логический if(true) else (false) == можно подписаться на label(0) и label(1) соответственно
	// false/true Можно организовать сразу более сложную логику с логическим ветвлением на std::int32_t вариантов (не
	// только false/true)
	virtual void subscribe_on_highway(std::weak_ptr<ISubscription<Publication>>, std::int32_t label = {}) = 0;
	virtual void subscribe_direct(std::weak_ptr<ISubscription<Publication>>, std::int32_t label = {}) = 0;
};

/**
 * @brief The INode class
 * Base class for execution tree nodes.
 * This interface can be accessed from callbacks.
 * @note thread safe
 */
class INode
{
public:
	INode(std::weak_ptr<INode> self_weak, HighWayProxyPtr highway, std::int32_t node_id)
		: self_weak_{std::move(self_weak)}
		, highway_{std::move(highway)}
		, node_id_{node_id}
	{
	}

	INode(std::weak_ptr<INode> self_weak, std::shared_ptr<HighWay> highway, std::int32_t node_id)
		: self_weak_{std::move(self_weak)}
		, highway_{hi::make_proxy(std::move(highway))}
		, node_id_{node_id}
	{
	}

	virtual ~INode() = default;

	void set_progress_publisher(std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher)
	{
		progress_publisher_ = std::move(progress_publisher);
	}

	std::int32_t node_id() const noexcept
	{
		return node_id_;
	}

	void publish_progress_state(std::int32_t achieved_progress) const noexcept
	{
		if (progress_publisher_)
		{
			progress_publisher_->publish(NodeProgress{node_id_, achieved_progress});
		}
	}

	// Удалить свои входящие каналы с меткой label
	void delete_in_channels_on_highway(std::int32_t label)
	{
		execute(
			[this, label]
			{
				delete_in_channels_impl(label);
			},
			__FILE__,
			__LINE__);
	}

	// Удалить свои входящие каналы с меткой label
	void delete_in_channels_direct(std::int32_t label)
	{
		delete_in_channels_impl(label);
	}

	// Удалить все свои входящие каналы
	virtual void delete_all_in_channels_on_highway()
	{
		execute(
			[this]
			{
				delete_all_in_channels_impl();
			},
			__FILE__,
			__LINE__);
	}

	// Удалить все свои входящие каналы
	virtual void delete_all_in_channels_direct()
	{
		delete_all_in_channels_impl();
	}

	// Удалить свои исходящие каналы с меткой label
	void delete_out_channels_on_highway(std::int32_t label)
	{
		execute(
			[this, label]
			{
				delete_out_channels_impl(label);
			},
			__FILE__,
			__LINE__);
	}

	// Удалить свои исходящие каналы с меткой label
	void delete_out_channels_direct(std::int32_t label)
	{
		delete_out_channels_impl(label);
	}

	// Удалить все свои исходящие каналы
	void delete_all_out_channels_on_highway()
	{
		execute(
			[this]
			{
				delete_all_out_channels_impl();
			},
			__FILE__,
			__LINE__);
	}

	// Удалить все свои исходящие каналы
	void delete_all_out_channels_direct()
	{
		execute(
			[this]
			{
				delete_all_out_channels_impl();
			},
			__FILE__,
			__LINE__);
	}

	/**
	 * Подключение результата этого узла на вход следующего узла
	 *
	 * @param next_node - следующий узел
	 * @param result_label - на какой номер должна быть публикация чтобы она была отправлена:
	 * используется в if-else и потобном ветвлении чтобы отправить результат в рамкой логической ветки с номером
	 * result_label
	 * @param label_in_next - каким номером должен быть промечен входящий сигнал в следующий узел:
	 * номер может не совпадать с номером result_label
	 * @note send_may_fail  - данная передача идёт с перепланированием на потоке принимающей стороне,
	 * поэтому указывается обязательна ли передача (может быть блокирующее ожидание если на хайвее нехватка холдеров).
	 * send_may_fail == true : если на хайвее нехватка холдеров, то можно пропустить передачу
	 * send_may_fail == false : передача обязательна, если нехватка холдеров то ждать когда освободятся
	 */
	template <typename OutResult>
	bool connect_to_highway_channel(
		INode * next_node,
		const std::int32_t result_label,
		const std::int32_t label_in_next,
		bool send_may_fail = true)
	{
		if (!next_node)
			return false;
		InParamChannel<OutResult> * next_node_in = dynamic_cast<InParamChannel<OutResult> *>(next_node);
		if (!next_node_in)
			return false;
		Publisher<OutResult> * result = dynamic_cast<Publisher<OutResult> *>(this);
		if (!result)
			return false;
		result->subscribe_on_highway(
			next_node_in->in_param_highway_channel(label_in_next, send_may_fail),
			result_label);
		return true;
	}

	/**
	 * Подключение результата этого узла на вход следующего узла
	 *
	 * @param next_node - следующий узел
	 * @param result_label - на какой номер должна быть публикация чтобы она была отправлена:
	 * используется в if-else и потобном ветвлении чтобы отправить результат в рамкой логической ветки с номером
	 * result_label
	 * @param label_in_next - каким номером должен быть промечен входящий сигнал в следующий узел:
	 * номер может не совпадать с номером result_label
	 */
	template <typename OutResult>
	bool connect_to_direct_channel(INode * next_node, const std::int32_t result_label, const std::int32_t label_in_next)
	{
		if (!next_node)
			return false;
		InParamChannel<OutResult> * next_node_in = dynamic_cast<InParamChannel<OutResult> *>(next_node);
		if (!next_node_in)
			return false;
		Publisher<OutResult> * result = dynamic_cast<Publisher<OutResult> *>(this);
		if (!result)
			return false;
		result->subscribe_direct(next_node_in->in_param_direct_channel(label_in_next), result_label);
		return true;
	}

protected:
	// Запустить на highway_ под защитой self_weak_
	template <typename R>
	void execute(R && r, const char * filename, unsigned int line)
	{
		highway_->execute(Runnable::create(std::move(r), self_weak_, filename, line));
	}

	// Удалить свои входящие каналы с меткой label
	virtual void delete_in_channels_impl(std::int32_t label) = 0;

	// Удалить все свои входящие каналы
	virtual void delete_all_in_channels_impl() = 0;

	// Удалить свои исходящие каналы с меткой label
	virtual void delete_out_channels_impl(std::int32_t label) = 0;

	// Удалить все свои исходящие каналы
	virtual void delete_all_out_channels_impl() = 0;

protected:
	const std::weak_ptr<INode> self_weak_;
	const HighWayProxyPtr highway_;
	const std::int32_t node_id_;

	std::shared_ptr<HighWayStickyPublisher<NodeProgress>> progress_publisher_;
};

using INodePtr = std::shared_ptr<INode>;

} // namespace hi

#endif // THREADS_HIGHWAYS_EXECUTION_TREE_INODE_H
