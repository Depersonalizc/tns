#include "src/util/periodic_thread.hpp"
#include "util/defines.hpp"

#include <chrono>

namespace tns {

util::threading::PeriodicThread::PeriodicThread(const std::chrono::nanoseconds &period, 
                                                const std::function<void()>& task)
{
    ::THROW_NO_IMPL();
}

util::threading::PeriodicThread::~PeriodicThread()
{
    ::THROW_NO_IMPL();
}

} // namespace tns
