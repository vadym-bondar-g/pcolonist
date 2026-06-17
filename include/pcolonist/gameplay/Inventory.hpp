#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace pcolonist {

enum class Tool {
    Axe,
    Empty,
};

class Inventory {
public:
    static constexpr std::size_t hotbarSize = 5;

    Inventory();

    void select(std::size_t slot);
    void addWood(int amount);
    void addStone(int amount);
    void addWater(int amount);
    bool spendWood(int amount);
    bool spendStone(int amount);

    [[nodiscard]] std::size_t selectedSlot() const;
    [[nodiscard]] Tool selectedTool() const;
    [[nodiscard]] std::string_view toolName(std::size_t slot) const;
    [[nodiscard]] int wood() const;
    [[nodiscard]] int stone() const;
    [[nodiscard]] int water() const;

private:
    std::array<Tool, hotbarSize> tools_{};
    std::size_t selectedSlot_ = 0;
    int wood_ = 0;
    int stone_ = 0;
    int water_ = 0;
};

} // namespace pcolonist
