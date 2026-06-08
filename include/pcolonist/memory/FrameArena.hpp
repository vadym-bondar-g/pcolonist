#pragma once

#include <cstddef>
#include <memory_resource>
#include <vector>

namespace pcolonist {

class FrameArena {
public:
    explicit FrameArena(std::size_t capacity = 1024 * 1024);

    void reset();
    [[nodiscard]] std::pmr::memory_resource* resource();
    [[nodiscard]] std::size_t capacity() const;

private:
    std::vector<std::byte> storage_;
    std::pmr::monotonic_buffer_resource resource_;
};

} // namespace pcolonist
