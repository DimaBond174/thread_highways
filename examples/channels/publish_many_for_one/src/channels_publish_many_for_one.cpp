#include <thread_highways/include_all.h>
#include <thread_highways/tools/cout_scope.h>

#include <vector>

using namespace std::chrono_literals;

void test_with_thread_names()
{
    hi::CoutScope scope("channels_publish_many_for_one");

    // RAIIdestroy will automatically destroy the highway when exiting the scope
        hi::RAIIdestroy highway{ hi::make_self_shared<hi::HighWay>() };

        highway->execute(
        []()
        {
            // Just a handy thing for debugging - you can give names to threads
                        hi::set_this_thread_name("highway7");
        },
        __FILE__,
                __LINE__);

    auto publisher = std::make_shared<hi::PublishManyForOne<std::string>>(
        [&](std::string publication)
        {
            scope.print(hi::get_this_thread_name() + ": " + std::move(publication));
        }, highway.object_, __FILE__, __LINE__, false, false
    );

    const auto fun = [&](std::uint32_t id)
    {
        std::string str{"worker"};
        str.append(std::to_string(id));

        hi::set_this_thread_name(str);
        str.append(" data : ");

        for (int i = 0; i <= 50; ++i)
        {
                    publisher->publish(std::string{str}.append(std::to_string(i)));
                    std::this_thread::sleep_for(10ms);
        }
    };

    std::vector<std::thread> threads;
    for (std::uint32_t id = 0; id < 5; ++id)
    {
        threads.emplace_back(
            [&, id]
            {
                fun(id);
            });
    }

        publisher->publish(std::string{"Mother washed the frame"});

    for (auto && it : threads)
    {
        it.join();
    }

    highway->flush_tasks();
}

void test_duplicates(bool filter_duplicates)
{
    hi::CoutScope scope("channels_publish_many_for_one, filter_duplicates=" + std::to_string(filter_duplicates));

    // RAIIdestroy will automatically destroy the highway when exiting the scope
        hi::RAIIdestroy highway{ hi::make_self_shared<hi::HighWay>() };

        highway->execute(
        []()
        {
            // Just a handy thing for debugging - you can give names to threads
                        hi::set_this_thread_name("highway8");
        },
        __FILE__,
                __LINE__);

    auto publisher = std::make_shared<hi::PublishManyForOne<std::uint32_t>>(
        [&](std::uint32_t publication)
        {
            scope.print(hi::get_this_thread_name() + ": " + std::to_string(publication));
        }, highway.object_, __FILE__, __LINE__, false, filter_duplicates
    );

    const auto fun = [&]()
    {
        for (int i = 0; i <= 50; ++i)
        {
                    publisher->publish(i);
                    std::this_thread::sleep_for(10ms);
        }
    };

    std::vector<std::thread> threads;
    for (std::uint32_t id = 0; id < 5; ++id)
    {
        threads.emplace_back(
            [&]
            {
                fun();
            });
    }

    for (auto && it : threads)
    {
        it.join();
    }

    highway->flush_tasks();
}

// This is a very simple example of sending messages to one subscriber.
int main(int /* argc */, char ** /* argv */)
{
    test_with_thread_names();
    test_duplicates(false); // Must see publications with same value
    test_duplicates(true); // Must see only changes

	std::cout << "Test finished" << std::endl;
	return 0;
}
