#include "pcolonist/platform/Input.hpp"

#include "pcolonist/render/Camera.hpp"
#include "pcolonist/core/EventBus.hpp"

#include <GLFW/glfw3.h>

namespace pcolonist {

Input::Input(EventBus& events) {
    events.subscribe<KeyEvent>([this](const KeyEvent& event) {
        onKey(event);
    });
    events.subscribe<WindowCloseEvent>([this](const WindowCloseEvent&) {
        closeRequested_ = true;
    });
}

void Input::update(Camera& camera, float deltaTime) const {
    if (keys_[GLFW_KEY_W]) {
        camera.move(CameraMovement::Forward, deltaTime);
    }
    if (keys_[GLFW_KEY_S]) {
        camera.move(CameraMovement::Backward, deltaTime);
    }
    if (keys_[GLFW_KEY_A]) {
        camera.move(CameraMovement::Left, deltaTime);
    }
    if (keys_[GLFW_KEY_D]) {
        camera.move(CameraMovement::Right, deltaTime);
    }
    if (keys_[GLFW_KEY_SPACE]) {
        camera.move(CameraMovement::Up, deltaTime);
    }
    if (keys_[GLFW_KEY_LEFT_SHIFT]) {
        camera.move(CameraMovement::Down, deltaTime);
    }
}

bool Input::keyDown(int key) const {
    return key >= 0 && key < maxKeys && keys_[static_cast<std::size_t>(key)];
}

bool Input::closeRequested() const {
    return closeRequested_;
}

bool Input::cursorCaptured() const {
    return cursorCaptured_;
}

void Input::setCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
}

void Input::onKey(const KeyEvent& event) {
    if (event.key >= 0 && event.key < maxKeys) {
        keys_[static_cast<std::size_t>(event.key)] = event.action != KeyAction::Release;
    }

    if (event.key == GLFW_KEY_F1 && event.action == KeyAction::Press) {
        cursorCaptured_ = !cursorCaptured_;
    }
}

} // namespace pcolonist
