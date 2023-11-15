#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>


namespace tns {

namespace util::threading {
    class ThreadPool {
    public:
        explicit ThreadPool(std::size_t numThreads);
        ~ThreadPool();

        void enqueueTask(std::packaged_task<void()> task);

    private:
        // std::vector<std::thread> workers;
        std::vector<std::jthread> workers;
        std::queue<std::packaged_task<void()>> tasks;
        std::mutex tasksMutex;
        std::condition_variable cv;
        bool stop = false;
        void workerFunction();
    };
} // namespace util::threading

} // namespace tns
