#pragma once

#include "pcolonist/core/Events.hpp"

#include <array>

namespace pcolonist {

class Camera;
class EventBus;

class Input {
public:
    explicit Input(EventBus& events);

    void update(Camera& camera, float deltaTime) const;
    [[nodiscard]] bool keyDown(int key) const;
    [[nodiscard]] bool closeRequested() const;
    [[nodiscard]] bool cursorCaptured() const;
    void setCursorCaptured(bool captured);
    void clearMovementKeys();

private:
    void onKey(const KeyEvent& event);

    static constexpr int maxKeys = 512;
    std::array<bool, maxKeys> keys_{};
    bool closeRequested_ = false;
    bool cursorCaptured_ = true;
};

} // namespace pcolonist
