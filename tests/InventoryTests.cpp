#include "pcolonist/gameplay/Inventory.hpp"

#include <stdexcept>

void runInventoryTests() {
    pcolonist::Inventory inventory;
    if (inventory.selectedTool() != pcolonist::Tool::Axe || inventory.toolName(0) != "ТОПОР") {
        throw std::runtime_error("The axe must occupy the first hotbar slot");
    }
    inventory.addWood(5);
    inventory.select(1);
    if (inventory.wood() != 5 || inventory.selectedTool() != pcolonist::Tool::Empty) {
        throw std::runtime_error("Inventory must store resources and select hotbar slots");
    }
    if (!inventory.hasItem(pcolonist::ItemId::Wood, 5) || inventory.totalWeight() <= 0.0F || inventory.totalVolume() <= 0.0F) {
        throw std::runtime_error("Inventory must expose item stacks with weight and volume");
    }
    if (!inventory.addItem(pcolonist::ItemId::Fiber, 31) || inventory.itemCount(pcolonist::ItemId::Fiber) != 31) {
        throw std::runtime_error("Inventory must split large quantities across stack-limited slots");
    }
    const auto resources = inventory.filteredSlots(pcolonist::InventoryFilter::Resources);
    if (resources.size() < 2) {
        throw std::runtime_error("Inventory filters must return matching non-empty slots");
    }
    inventory.sort(pcolonist::InventorySortMode::Quantity);
    if (inventory.slots().front().empty() || inventory.slots().front().quantity < 5) {
        throw std::runtime_error("Inventory sort must keep populated slots first");
    }
    if (!inventory.spendResources(3, 0, 10, 0) || inventory.wood() != 2 || inventory.itemCount(pcolonist::ItemId::Fiber) != 21) {
        throw std::runtime_error("Inventory resource spending must remove item stack quantities");
    }
    if (!inventory.unlockTool(pcolonist::Tool::Knife) || !inventory.hasTool(pcolonist::Tool::Knife)
        || !inventory.hasItem(pcolonist::ItemId::Knife)) {
        throw std::runtime_error("Inventory must unlock tools as hotbar entries and item instances");
    }
}
