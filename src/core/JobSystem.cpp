#include "pcolonist/core/JobSystem.hpp"

#include <algorithm>

namespace pcolonist {

JobSystem::JobSystem(std::size_t workerCount) {
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index) {
        workers_.emplace_back([this] {
            workerLoop();
        });
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    workAvailable_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void JobSystem::waitIdle() {
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] {
        return jobs_.empty() && activeJobs_ == 0;
    });
}

std::size_t JobSystem::workerCount() const noexcept {
    return workers_.size();
}

std::size_t JobSystem::defaultWorkerCount() noexcept {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads <= 2) {
        return 1;
    }
    return std::min<std::size_t>(hardwareThreads - 1, 8);
}

void JobSystem::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            workAvailable_.wait(lock, [this] {
                return stopping_ || !jobs_.empty();
            });
            if (stopping_ && jobs_.empty()) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop();
            ++activeJobs_;
        }

        job();

        {
            std::lock_guard lock(mutex_);
            --activeJobs_;
            if (jobs_.empty() && activeJobs_ == 0) {
                idle_.notify_all();
            }
        }
    }
}

} // namespace pcolonist
