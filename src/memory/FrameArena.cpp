#include "pcolonist/memory/FrameArena.hpp"

namespace pcolonist {

FrameArena::FrameArena(std::size_t capacity)
    : storage_(capacity),
      resource_(storage_.data(), storage_.size()) {}

void FrameArena::reset() {
    resource_.release();
}

std::pmr::memory_resource* FrameArena::resource() {
    return &resource_;
}

std::size_t FrameArena::capacity() const {
    return storage_.size();
}

} // namespace pcolonist
