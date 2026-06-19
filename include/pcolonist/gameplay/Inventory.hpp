#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace pcolonist {

enum class Tool {
    Axe,
    Knife,
    Pickaxe,
    Torch,
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
    void addFiber(int amount);
    void addMetal(int amount);
    bool spendWood(int amount);
    bool spendStone(int amount);
    bool spendWater(int amount);
    bool spendFiber(int amount);
    bool spendMetal(int amount);
    bool spendResources(int wood, int stone, int fiber, int metal);
    bool unlockTool(Tool tool);

    [[nodiscard]] std::size_t selectedSlot() const;
    [[nodiscard]] Tool selectedTool() const;
    [[nodiscard]] std::string_view toolName(std::size_t slot) const;
    [[nodiscard]] bool hasTool(Tool tool) const;
    [[nodiscard]] bool canAfford(int wood, int stone, int fiber, int metal) const;
    [[nodiscard]] int wood() const;
    [[nodiscard]] int stone() const;
    [[nodiscard]] int water() const;
    [[nodiscard]] int fiber() const;
    [[nodiscard]] int metal() const;

private:
    std::array<Tool, hotbarSize> tools_{};
    std::size_t selectedSlot_ = 0;
    int wood_ = 0;
    int stone_ = 0;
    int water_ = 0;
    int fiber_ = 0;
    int metal_ = 0;
};

} // namespace pcolonist
