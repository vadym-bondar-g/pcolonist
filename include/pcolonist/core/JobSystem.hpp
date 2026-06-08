#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace pcolonist {

class JobSystem {
public:
    explicit JobSystem(std::size_t workerCount = defaultWorkerCount());
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    template <typename Function>
    auto submit(Function&& function) -> std::future<std::invoke_result_t<Function>> {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
        std::future<Result> result = task->get_future();

        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("Cannot submit a job while JobSystem is stopping");
            }
            jobs_.emplace([task] {
                (*task)();
            });
        }
        workAvailable_.notify_one();
        return result;
    }

    void waitIdle();

    [[nodiscard]] std::size_t workerCount() const noexcept;
    [[nodiscard]] static std::size_t defaultWorkerCount() noexcept;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    mutable std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable idle_;
    std::size_t activeJobs_ = 0;
    bool stopping_ = false;
};

} // namespace pcolonist
