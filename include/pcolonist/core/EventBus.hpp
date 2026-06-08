#pragma once

#include "pcolonist/core/Events.hpp"

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pcolonist {

using Event = std::variant<KeyEvent, MouseMovedEvent, MouseButtonEvent, WindowResizedEvent, WindowCloseEvent>;

class EventBus {
public:
    template <typename EventType, typename Callback>
    void subscribe(Callback&& callback) {
        static_assert(std::is_invocable_v<Callback, const EventType&>);
        subscribers_.emplace_back(
            [handler = std::forward<Callback>(callback)](const Event& event) {
                if (const auto* typedEvent = std::get_if<EventType>(&event)) {
                    std::invoke(handler, *typedEvent);
                }
            });
    }

    template <typename EventType>
    void enqueue(EventType&& event) {
        queue_.emplace_back(std::forward<EventType>(event));
    }

    void dispatch() {
        auto events = std::move(queue_);
        queue_.clear();

        for (const Event& event : events) {
            for (const Subscriber& subscriber : subscribers_) {
                subscriber(event);
            }
        }
    }

private:
    using Subscriber = std::function<void(const Event&)>;

    std::vector<Event> queue_;
    std::vector<Subscriber> subscribers_;
};

} // namespace pcolonist
