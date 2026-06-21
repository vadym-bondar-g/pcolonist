#include "pcolonist/gameplay/Inventory.hpp"

#include <algorithm>
#include <array>

namespace pcolonist {

namespace {

std::size_t slotForTool(Tool tool) {
    switch (tool) {
    case Tool::Axe:
        return 0;
    case Tool::Knife:
        return 1;
    case Tool::Pickaxe:
        return 2;
    case Tool::Torch:
        return 3;
    case Tool::Empty:
        return Inventory::hotbarSize;
    }
    return Inventory::hotbarSize;
}

} // namespace

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

void Inventory::addFiber(int amount) {
    fiber_ = std::max(fiber_ + amount, 0);
}

void Inventory::addMetal(int amount) {
    metal_ = std::max(metal_ + amount, 0);
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

bool Inventory::spendWater(int amount) {
    if (water_ < amount) {
        return false;
    }
    water_ -= amount;
    return true;
}

bool Inventory::spendFiber(int amount) {
    if (fiber_ < amount) {
        return false;
    }
    fiber_ -= amount;
    return true;
}

bool Inventory::spendMetal(int amount) {
    if (metal_ < amount) {
        return false;
    }
    metal_ -= amount;
    return true;
}

bool Inventory::spendResources(int wood, int stone, int fiber, int metal) {
    if (!canAfford(wood, stone, fiber, metal)) {
        return false;
    }
    wood_ -= wood;
    stone_ -= stone;
    fiber_ -= fiber;
    metal_ -= metal;
    return true;
}

bool Inventory::unlockTool(Tool tool) {
    const std::size_t slot = slotForTool(tool);
    if (slot >= tools_.size() || tools_[slot] == tool) {
        return false;
    }
    tools_[slot] = tool;
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
    switch (tools_[slot]) {
    case Tool::Axe:
        return "ТОПОР";
    case Tool::Knife:
        return "НОЖ";
    case Tool::Pickaxe:
        return "КИРКА";
    case Tool::Torch:
        return "ФАКЕЛ";
    case Tool::Empty:
        return "";
    }
    return "";
}

bool Inventory::hasTool(Tool tool) const {
    return std::find(tools_.begin(), tools_.end(), tool) != tools_.end();
}

bool Inventory::canAfford(int wood, int stone, int fiber, int metal) const {
    return wood_ >= wood && stone_ >= stone && fiber_ >= fiber && metal_ >= metal;
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

int Inventory::fiber() const {
    return fiber_;
}

int Inventory::metal() const {
    return metal_;
}

InventorySnapshot Inventory::snapshot() const {
    return {
        {
            hasTool(Tool::Axe),
            hasTool(Tool::Knife),
            hasTool(Tool::Pickaxe),
            hasTool(Tool::Torch),
        },
        selectedSlot_,
        wood_,
        stone_,
        water_,
        fiber_,
        metal_,
    };
}

void Inventory::applySnapshot(const InventorySnapshot& snapshot) {
    tools_.fill(Tool::Empty);
    if (snapshot.tools[0]) {
        tools_[slotForTool(Tool::Axe)] = Tool::Axe;
    }
    if (snapshot.tools[1]) {
        tools_[slotForTool(Tool::Knife)] = Tool::Knife;
    }
    if (snapshot.tools[2]) {
        tools_[slotForTool(Tool::Pickaxe)] = Tool::Pickaxe;
    }
    if (snapshot.tools[3]) {
        tools_[slotForTool(Tool::Torch)] = Tool::Torch;
    }
    selectedSlot_ = snapshot.selectedSlot < hotbarSize ? snapshot.selectedSlot : 0;
    wood_ = std::max(snapshot.wood, 0);
    stone_ = std::max(snapshot.stone, 0);
    water_ = std::max(snapshot.water, 0);
    fiber_ = std::max(snapshot.fiber, 0);
    metal_ = std::max(snapshot.metal, 0);
}

} // namespace pcolonist
