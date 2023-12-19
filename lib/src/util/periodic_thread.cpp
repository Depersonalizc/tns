#include "util/periodic_thread.hpp"

#include <chrono>

namespace tns {
namespace util::threading {

PeriodicThread::PeriodicThread(const nsec &period, std::function<void()>&& task)
    : thread{[this, task = std::move(task), period] {
        using SC = std::chrono::steady_clock;
        auto prev = SC::now();
        while (!stopped) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                const auto waitTime = period - (SC::now() - prev);
                cv.wait_for(lock, waitTime, [this] { return stopped; });
                prev = SC::now();
                if (stopped)
                    return;
            }
            task();
        }
    }}
{}

PeriodicThread::~PeriodicThread() { stop(); std::cout << "PeriodicThread: DESTRUCTED\n";}

void PeriodicThread::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped) return;
        stopped = true;
    }
    cv.notify_one();
}


} // namespace util::threading
} // namespace tns
