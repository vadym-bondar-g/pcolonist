#pragma once

namespace pcolonist {

enum class KeyAction {
    Press,
    Release,
    Repeat,
};

struct KeyEvent {
    int key = 0;
    KeyAction action = KeyAction::Release;
};

struct MouseMovedEvent {
    double x = 0.0;
    double y = 0.0;
};

struct MouseButtonEvent {
    int button = 0;
    KeyAction action = KeyAction::Release;
    double x = 0.0;
    double y = 0.0;
};

struct WindowResizedEvent {
    int width = 0;
    int height = 0;
};

struct WindowCloseEvent {};

} // namespace pcolonist
