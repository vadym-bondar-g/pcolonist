#include "pcolonist/core/FrameLimiter.hpp"

#include <thread>

namespace pcolonist {

void FrameLimiter::beginFrame() {
    frameStart_ = Clock::now();
}

void FrameLimiter::endFrame() const {
    if (limit_ <= 0) {
        return;
    }
    const auto duration = std::chrono::duration<double>(1.0 / static_cast<double>(limit_));
    const auto target = frameStart_ + std::chrono::duration_cast<Clock::duration>(duration);
    constexpr auto spinMargin = std::chrono::microseconds(500);
    if (Clock::now() + spinMargin < target) {
        std::this_thread::sleep_until(target - spinMargin);
    }
    while (Clock::now() < target) {
        std::this_thread::yield();
    }
}

void FrameLimiter::setLimit(int framesPerSecond) {
    limit_ = framesPerSecond;
}

int FrameLimiter::limit() const {
    return limit_;
}

} // namespace pcolonist
