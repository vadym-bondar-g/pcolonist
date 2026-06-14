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
    return tools_[slot] == Tool::Axe ? "AXE" : "";
}

int Inventory::wood() const {
    return wood_;
}

} // namespace pcolonist
