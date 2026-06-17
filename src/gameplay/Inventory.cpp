#include "pcolonist/gameplay/Inventory.hpp"

#include <algorithm>

namespace pcolonist {

Inventory::Inventory() {
    tools_.fill(Tool::Empty);
    tools_[0] = Tool::Axe;
}

void Inventory::select(std::size_t slot) {
    if (slot < tools_.size()) {
        selectedSlot_ = slot;
    }
}

void Inventory::addWood(int amount) {
    wood_ = std::max(wood_ + amount, 0);
}

void Inventory::addStone(int amount) {
    stone_ = std::max(stone_ + amount, 0);
}

void Inventory::addWater(int amount) {
    water_ = std::max(water_ + amount, 0);
}

bool Inventory::spendWood(int amount) {
    if (wood_ < amount) {
        return false;
    }
    wood_ -= amount;
    return true;
}

bool Inventory::spendStone(int amount) {
    if (stone_ < amount) {
        return false;
    }
    stone_ -= amount;
    return true;
}

std::size_t Inventory::selectedSlot() const {
    return selectedSlot_;
}

Tool Inventory::selectedTool() const {
    return tools_[selectedSlot_];
}

std::string_view Inventory::toolName(std::size_t slot) const {
    if (slot >= tools_.size()) {
        return {};
    }
    return tools_[slot] == Tool::Axe ? "ТОПОР" : "";
}

int Inventory::wood() const {
    return wood_;
}

int Inventory::stone() const {
    return stone_;
}

int Inventory::water() const {
    return water_;
}

} // namespace pcolonist
