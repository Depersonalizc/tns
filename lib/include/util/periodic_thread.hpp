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
    using nsec = std::chrono::nanoseconds;
public:
    PeriodicThread() = default;
    PeriodicThread(const nsec &period, std::function<void()>&& task);

    template <typename Callable, typename... Args>
    PeriodicThread(const nsec &period, Callable&& f, Args&&... args)
        : PeriodicThread{period, std::function<void()>{std::bind(std::forward<Callable>(f), std::forward<Args>(args)...)}}
    {}

    // PeriodicThread(const PeriodicThread&) = delete;
    // PeriodicThread(PeriodicThread&& other)
    //     : thread{std::move(other.thread)}
    //     , mutex{std::exchange(other.mutex, {})}
    //     , cv{std::exchange(other.cv, {})}
    //     , stop{std::exchange(other.stop, false)}
    // {}

    // PeriodicThread& operator=(const PeriodicThread&) = delete;
    // PeriodicThread& operator=(PeriodicThread&& other)
    // {
    //     if (this != &other) {
    //         thread = std::move(other.thread);
    //         mutex = std::exchange(other.mutex, {});
    //         cv = std::exchange(other.cv, {});
    //         stop = std::exchange(other.stop, false);
    //     }
    //     return *this;
    // }

    ~PeriodicThread();

    void stop();

private:
    std::jthread thread;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    bool stopped = false;
};


} // namespace util::threading
} // namespace tns
