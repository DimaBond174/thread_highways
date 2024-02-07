#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <atomic>
#include <future>
#include <optional>

// Счётчик прогресса
struct Progress
{
    Progress() : progress_{0u}, finished_{false} {}
    Progress(const std::uint32_t progress, const bool finished) : progress_{progress}, finished_{finished} {}

    bool operator==(const Progress& other)
    {
        return other.finished_ == finished_ && other.progress_ == progress_;
    }

    std::uint32_t progress_; // Прогресс в единицах
    bool finished_; // Завершена ли работа
};

// Процесс с прогрессом (например анализатор файлов)
struct ProgressPublisher
{
    // Логика функтора
    void operator()(
        const std::atomic<bool>& keep_run // флаг пытаются ли поток уже остановить (опциональный параметр)
        )
    {
        for (std::uint32_t progress = 0u; progress < 12345u
            && keep_run.load(std::memory_order_acquire); // Пока не начали останавливать поток
             ++progress)
        {
            publisher_->publish(Progress{progress, false});
        }
        publisher_->publish(Progress{12345u, true});
    }

    std::shared_ptr<hi::PublishOneForMany<Progress>> publisher_ = hi::make_self_shared<hi::PublishOneForMany<Progress>>();
};

// Счётчик объектов которые обрабатывает ProgressPublisher
// Определяет сколько объектов - это 100%
struct CountPublisher
{
    // Логика функтора
    void operator()(  )
    {
        publisher_->publish(12777u); // может не совпадать с 12345u, например файлов было/стало больше
    }

    std::shared_ptr<hi::PublishOneForMany<std::uint32_t>> publisher_ = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
};

struct PercentPublisher
{
    PercentPublisher(ProgressPublisher& progress_publisher, CountPublisher& count_publisher, hi::HighWayProxyPtr highway)
    {
        // Подписываюсь на сигналы о прогрессе
        progress_subscription_ =progress_publisher.publisher_->subscribe_channel()->subscribe(
            [&](Progress publication)
            {
                // Так как указан однопоточный highway, мьютексы не нужны - всё будет приходить в одном потоке
                last_progress_ = publication;
                if (publication.finished_)
                {
                    publisher_->publish(100u);
                    return;
                }

                if (!all_count_) return;

                auto all_count = *all_count_;
                if (all_count < publication.progress_)
                {
                    // Чтобы не получилось 146%
                    all_count = publication.progress_;
                }

                publisher_->publish((100u * publication.progress_) / all_count);
            }
            , highway // поток в котором будут приходить сигналы
            , __FILE__, __LINE__
            , false /* send_may_fail - нельзя забивать на доставку сигналов */
            , true /* for_new_only - фильтровать сигналы с одинаковыми данными */
        );

        // Подписываюсь на сигналы счётчика
        count_subscription_ = count_publisher.publisher_->subscribe_channel()->subscribe(
            [&](std::uint32_t publication)
            {
                // Так как указан однопоточный highway, мьютексы не нужны - всё будет приходить в одном потоке
                all_count_ = publication;

                if (!last_progress_ || last_progress_->finished_) return;

                if (publication < last_progress_->progress_)
                {
                    // Чтобы не получилось 146%
                    publication = last_progress_->progress_;
                }

                publisher_->publish((100u * last_progress_->progress_) / publication);
            }
            , highway // поток в котором будут приходить сигналы
            , __FILE__, __LINE__
            , true /* send_may_fail - можно забивать на доставку сигналов т.к. подписка progress_subscription_ 100% сможет доставить*/
            , false /* for_new_only - не буду фильтровать сигналы с одинаковыми данными, для примера */
        );
    }

    std::shared_ptr<hi::ISubscription<Progress>> progress_subscription_;
    std::shared_ptr<hi::ISubscription<std::uint32_t>> count_subscription_;

    std::optional<Progress> last_progress_{};
    std::optional<std::uint32_t> all_count_{};

    std::shared_ptr<hi::PublishOneForMany<std::uint32_t>> publisher_ = hi::make_self_shared<hi::PublishOneForMany<std::uint32_t>>();
};

void scan_files(bool need_to_start_counter)
{
    hi::CoutScope scope(std::string{"scan_files, need_to_start_counter="}.append(std::to_string(need_to_start_counter)));

    // Ферма потоков на которую можно кидать задачи для запуска
    hi::RAIIdestroy multi_threaded_farm{hi::make_self_shared<hi::MultiThreadedTaskProcessingPlant>(
            4 /* threads */,
            [&](const hi::Exception& ex)
            {
                // Что делать с эксепшонами которые кидают исполняемые таски
                // В данном примере просто логирую
                scope.print(ex.what_as_string() + " in " + ex.file_name() + ": " + std::to_string(ex.file_line()));
            } /* Exceptions handler */)};

    // Однопоточный обработчик задач
    hi::RAIIdestroy highway{  hi::make_self_shared<hi::HighWay>( /* тоже можно тюнинговать параметрами, но для минимального примера не буду */ ) };

    auto progress_publisher = std::make_shared<ProgressPublisher>();
    auto count_publisher = std::make_shared<CountPublisher>();

    // Связываю узлы логики между собой
    PercentPublisher percent_publisher{*progress_publisher, *count_publisher, hi::make_proxy(*highway)};
    auto channel = percent_publisher.publisher_->subscribe_channel();

    // Вывод % в GUI
    auto printer = channel->subscribe(
            [&](std::uint32_t publication)
            {
                scope.print(std::string{"Progress: "}.append(std::to_string(publication)).append("%"));
            }, true /* for_new_only = true*/
    );

    // Ожидатор 100%
    std::promise<std::uint32_t> promise;
    auto future = promise.get_future();
    auto waiter = channel->subscribe(
            [&](std::uint32_t publication)
            {
                if (publication == 100u)
                {
                    promise.set_value(publication);
                }
            }, false /* for_new_only = false*/
    );

    /*
     *  Итого визуально можно представить:
     *  2 входящих канала в PercentPublisher через которые поступает информация о текущем прогрессе и общий счётчик объектов
     *  2 исходящих канала из PercentPublisher - в GUI печать и для ожидания main thread завершения теста
    */

    // ***************************
    // Запуск
    if (need_to_start_counter)
    {
        // Необязательный запуск (если лимит тасок превышен)
        multi_threaded_farm->try_execute(std::move(count_publisher));
    }

    // Обязательный запуск
    multi_threaded_farm->execute(std::move(progress_publisher));

    // Дождёмся завершения теста.. он же не зависнет если count_publisher не стартовал? ;))
    future.get();
}
