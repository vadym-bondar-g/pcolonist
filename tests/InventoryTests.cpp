#include "pcolonist/gameplay/Inventory.hpp"

#include <stdexcept>

void runInventoryTests() {
    pcolonist::Inventory inventory;
    if (inventory.selectedTool() != pcolonist::Tool::Axe || inventory.toolName(0) != "AXE") {
        throw std::runtime_error("The axe must occupy the first hotbar slot");
    }
    inventory.addWood(5);
    inventory.select(1);
    if (inventory.wood() != 5 || inventory.selectedTool() != pcolonist::Tool::Empty) {
        throw std::runtime_error("Inventory must store resources and select hotbar slots");
    }
}
