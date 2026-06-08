#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pcolonist {

enum class FrameStage : std::uint8_t {
    PollEvents,
    DispatchEvents,
    Input,
    Update,
    Render,
    Present,
};

struct FrameContext {
    float deltaTime = 0.0F;
    double totalTime = 0.0;
    std::uint64_t frameNumber = 0;
};

class FramePipeline {
public:
    using Task = std::function<void(FrameContext&)>;

    void add(FrameStage stage, std::string name, Task task);
    void execute(FrameContext& context) const;

private:
    struct Entry {
        FrameStage stage;
        std::string name;
        Task task;
    };

    std::vector<Entry> entries_;
};

} // namespace pcolonist
