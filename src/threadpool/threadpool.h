#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<iostream>
#include<vector>
#include<mutex>
#include<thread>
#include<functional>
#include<condition_variable>
#include<queue>
#include<future>

class ThreadPool
{
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv_worker;
    bool stop;
    ThreadPool();
public:
    static ThreadPool& getInstance()
    {
        static ThreadPool instance;
        return instance;
    }
    ~ThreadPool();

    template<typename F,typename... Args>
    auto addTask(F&&f,Args&&...arg)
        ->std::future<std::invoke_result_t<F,Args...>>
    {
        using r_type = std::invoke_result_t<F, Args...>;
        auto p_task = std::make_shared<std::packaged_task<r_type()>>
            (std::bind(std::forward<F>(f),std::forward<Args>(arg)...));
        std::future<r_type> out = p_task->get_future();
        {
            std::unique_lock<std::mutex> ulk(mtx);
            if (tasks.size() >= 100000)
            {
                //抛出异常或等待
                throw std::runtime_error("Task queue is full");
            }

            tasks.emplace(std::move([p_task]() { (*p_task)(); }));
        }
        cv_worker.notify_one();
        return out;
    }
};

#endif // THREADPOOL_H
