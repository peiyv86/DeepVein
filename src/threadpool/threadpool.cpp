#include "threadpool.h"

ThreadPool::ThreadPool() :stop(false)
{
    int threadCount = std::max(2, (int)std::thread::hardware_concurrency() - 5);
    threads.reserve(threadCount);
    for (int i = 0;i < threadCount;i++)
    {
        threads.emplace_back([this]() {
            while (true)
            {
                std::function<void()>task;
                {
                    std::unique_lock<std::mutex> ulk(mtx);
                    //任务不为空时继续，stop时继续（防阻塞）
                    cv_worker.wait(ulk, [this] {return !tasks.empty() || stop;});
                    if (tasks.empty() && stop)return;
                    task = tasks.front();
                    tasks.pop();
                }
                try
                {
                    task();
                }
                catch (...)
                {
                    //接入异常处理模块
                }
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> ulk(mtx);
        stop = true;
    }
    cv_worker.notify_all();
    for (auto& t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    std::cout << "threads end...";
}


