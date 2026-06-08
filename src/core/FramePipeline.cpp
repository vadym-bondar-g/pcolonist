#include "pcolonist/core/FramePipeline.hpp"

#include <algorithm>
#include <utility>

namespace pcolonist {

void FramePipeline::add(FrameStage stage, std::string name, Task task) {
    entries_.push_back({stage, std::move(name), std::move(task)});
    std::stable_sort(
        entries_.begin(),
        entries_.end(),
        [](const Entry& left, const Entry& right) {
            return left.stage < right.stage;
        });
}

void FramePipeline::execute(FrameContext& context) const {
    for (const Entry& entry : entries_) {
        entry.task(context);
    }
}

} // namespace pcolonist
