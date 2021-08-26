
#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>
#include <semaphore>

#include "MPMC Queue.h"


namespace PDL
{

    //defaults
    constexpr size_t EXECUTOR_THREAD_COUNT = 3; //number of created threads
    constexpr size_t EXECUTOR_QUEUE_CAPACITY = 20; //order capacity

    //config struct used to config a new threadpool instance
    struct ExecutorConfig
    {
        std::size_t
            capacity,
            threadCount;
    };

    class Executor
    {

    public:

        explicit Executor(ExecutorConfig* creationConfig = nullptr)
            :orderSignal(0), done(false)
        {
            if (!executorConfig)
            {
                config.threadCount = EXECUTOR_THREAD_COUNT;
                config.capacity = EXECUTOR_QUEUE_CAPACITY;
            }
            else
            {
                config = *creationConfig;
            }
            queue = std::make_unique<rigtorp::MPMCQueue<std::unique_ptr<OrderBase>>>(config.capacity);
            threads.resize(config.threadCount);

            std::size_t index = 0;
            for (auto& thread : threads)
            {
                thread.thread = std::jthread(&Executor::ThreadLoop, this, index);
                thread.token = thread.thread.get_stop_token();
                index++;
            }
        }

        ~Executor()
        {
            done.store(true);

            for (auto& thread : threads)
                thread.thread.request_stop();

            for (auto& thread : threads)
                thread.thread.join();
        }

        Executor(Executor& other) = delete;
        Executor& operator=(const Executor& other) = delete;

        template<class Function, class... Args>
        auto Execute(Function, Args&&...);

    private:

        void ThreadLoop(std::size_t index)
        {
            while (!done.load() && !threads[index].token.stop_requested())
            {
                orderSignal.acquire();
                std::unique_ptr<OrderBase> order;
                queue->try_pop(order);
                (*order)();
            }
        }

        struct OrderBase
        {
            virtual void operator()() = 0;
            virtual ~OrderBase() {}
        };

        template <class Callable>
        struct Order : OrderBase
        {
            Order(Callable&& target)
                :callable(std::forward<Callable>(target))
            {}

            void operator()() override
            {
                callable();
            }

            Callable callable;
        };

        template <class Callable>
        static std::unique_ptr<OrderBase> CreateOrder(Callable&& target)
        {
            return std::unique_ptr<OrderBase>(
                new Order<Callable>(std::forward<Callable>(target))
                );
        }

        struct ExecutorThread
        {
            std::jthread thread;
            std::stop_token token;
        };

        std::unique_ptr<rigtorp::MPMCQueue<std::unique_ptr<OrderBase>>> queue;
        std::vector<ExecutorThread> threads;
        ExecutorConfig config;
        std::atomic_bool done;
        std::counting_semaphore<> orderSignal;

    };

    template<class Function, class ...Args>
    auto Executor::Execute(Function target, Args && ...args)
    {
        std::packaged_task<std::invoke_result_t<Function, Args...>()> task(std::bind(target, args...));
        auto future = task.get_future();
        if (
            queue->try_push(std::move(CreateOrder(
                [order(std::move(task))]() mutable { order(); })))
            )
        {
            orderSignal.release();
        }
        return future;
    }

}
