#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#include <iostream>

namespace tns {

namespace util::threading {
    class PeriodicThread {
    public:
        PeriodicThread(const std::chrono::nanoseconds &period, const std::function<void()>& task);
        ~PeriodicThread();

    private:
        std::thread thread;
        std::mutex mutex;
        std::condition_variable cv;
        bool stop = false;
    };
} // namespace util::threading

} // namespace tns
