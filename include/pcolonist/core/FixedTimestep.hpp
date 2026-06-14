#pragma once

#include <algorithm>

namespace pcolonist {

class FixedTimestep {
public:
    explicit FixedTimestep(float step = 1.0F / 120.0F, int maximumSteps = 8)
        : step_(step),
          maximumSteps_(maximumSteps) {}

    template <typename Callback>
    int advance(float deltaTime, Callback&& callback) {
        accumulator_ = std::min(accumulator_ + static_cast<double>(deltaTime), step_ * maximumSteps_);
        int steps = 0;
        while (accumulator_ >= step_ && steps < maximumSteps_) {
            callback(step_);
            accumulator_ -= step_;
            ++steps;
        }
        return steps;
    }

    [[nodiscard]] float step() const {
        return step_;
    }

    [[nodiscard]] float alpha() const {
        return static_cast<float>(accumulator_ / step_);
    }

private:
    double accumulator_ = 0.0;
    double step_;
    int maximumSteps_;
};

} // namespace pcolonist
