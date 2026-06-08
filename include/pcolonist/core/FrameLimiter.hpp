#pragma once

#include <chrono>

namespace pcolonist {

class FrameLimiter {
public:
    void beginFrame();
    void endFrame() const;
    void setLimit(int framesPerSecond);
    [[nodiscard]] int limit() const;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point frameStart_ = Clock::now();
    int limit_ = 120;
};

} // namespace pcolonist
