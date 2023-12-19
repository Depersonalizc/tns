#include "src/util/thread_pool.hpp"

namespace tns {

util::threading::ThreadPool::ThreadPool(std::size_t numThreads)
{
    for (std::size_t i = 0; i < numThreads; ++i)
        workers.emplace_back(&util::threading::ThreadPool::workerFunction, this);
}

util::threading::ThreadPool::~ThreadPool() 
{
    {
        std::lock_guard<std::mutex> lock(tasksMutex);
        stop = true;
    }
    cv.notify_all();
    // for (auto& worker: workers)
    //     worker.join();
}

void util::threading::ThreadPool::enqueueTask(std::packaged_task<void()> task) 
{
    {
        std::lock_guard<std::mutex> lock(tasksMutex);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void util::threading::ThreadPool::workerFunction() 
{
    while (!stop)
    {
        std::packaged_task<void()> task;
        {
            std::unique_lock<std::mutex> lock(tasksMutex);

            cv.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty())  // lock reacquired
                return;

            // Get a new task from the queue
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

} // namespace tns
