#include "src/util/thread_pool.hpp"
#include "util/defines.hpp"

namespace tns {

util::threading::ThreadPool::ThreadPool(std::size_t numThreads)
{
    ::THROW_NO_IMPL();
}

util::threading::ThreadPool::~ThreadPool() 
{
    ::THROW_NO_IMPL();
}

void util::threading::ThreadPool::enqueueTask(std::packaged_task<void()> task) 
{
    ::THROW_NO_IMPL();
}

void util::threading::ThreadPool::workerFunction() 
{
    ::THROW_NO_IMPL();
}

} // namespace tns
