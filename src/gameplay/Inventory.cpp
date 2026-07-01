#include "pcolonist/gameplay/Inventory.hpp"

#include <algorithm>
#include <array>

namespace pcolonist {

namespace {

constexpr std::array itemDefinitions{
    ItemDefinition{ItemId::Empty, "", ItemCategory::Resource, ItemRarity::Common, 0, 0.0F, 0.0F, 0.0F},
    ItemDefinition{ItemId::Wood, "ДЕРЕВО", ItemCategory::Resource, ItemRarity::Common, 20, 0.65F, 1.15F, 0.0F},
    ItemDefinition{ItemId::Stone, "КАМЕНЬ", ItemCategory::Resource, ItemRarity::Common, 20, 1.10F, 0.75F, 0.0F},
    ItemDefinition{ItemId::CleanWater, "ВОДА", ItemCategory::Consumable, ItemRarity::Common, 10, 1.00F, 1.00F, 0.0F},
    ItemDefinition{ItemId::Fiber, "ВОЛОКНО", ItemCategory::Resource, ItemRarity::Common, 30, 0.12F, 0.20F, 0.0F},
    ItemDefinition{ItemId::Metal, "МЕТАЛЛ", ItemCategory::Resource, ItemRarity::Uncommon, 15, 1.35F, 0.45F, 0.0F},
    ItemDefinition{ItemId::Axe, "ТОПОР", ItemCategory::Tool, ItemRarity::Common, 1, 2.40F, 3.00F, 100.0F},
    ItemDefinition{ItemId::Knife, "НОЖ", ItemCategory::Tool, ItemRarity::Common, 1, 0.55F, 0.35F, 80.0F},
    ItemDefinition{ItemId::Pickaxe, "КИРКА", ItemCategory::Tool, ItemRarity::Uncommon, 1, 3.20F, 3.40F, 120.0F},
    ItemDefinition{ItemId::Torch, "ФАКЕЛ", ItemCategory::Tool, ItemRarity::Common, 1, 0.75F, 1.40F, 45.0F},
};

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

bool categoryMatches(ItemCategory category, InventoryFilter filter) {
    switch (filter) {
    case InventoryFilter::All:
        return true;
    case InventoryFilter::Resources:
        return category == ItemCategory::Resource;
    case InventoryFilter::Consumables:
        return category == ItemCategory::Consumable;
    case InventoryFilter::Tools:
        return category == ItemCategory::Tool;
    }
    return true;
}

int categoryRank(ItemCategory category) {
    switch (category) {
    case ItemCategory::Tool:
        return 0;
    case ItemCategory::Consumable:
        return 1;
    case ItemCategory::Resource:
        return 2;
    case ItemCategory::Equipment:
        return 3;
    case ItemCategory::Quest:
        return 4;
    }
    return 5;
}

} // namespace

Inventory::Inventory() {
    tools_.fill(Tool::Empty);
    tools_[0] = Tool::Axe;
    static_cast<void>(addItem(ItemId::Axe, 1));
}

void Inventory::select(std::size_t slot) {
    if (slot < tools_.size()) {
        selectedSlot_ = slot;
    }
}

bool Inventory::addItem(ItemId item, int quantity, float quality) {
    if (item == ItemId::Empty || quantity <= 0 || !canFitAdditional(item, quantity)) {
        return false;
    }

    int remaining = quantity;
    const ItemDefinition& itemDefinition = definition(item);
    const float clampedQuality = std::clamp(quality, 0.0F, 1.0F);
    for (InventorySlot& slot : slots_) {
        if (remaining <= 0) {
            break;
        }
        if (slot.item != item || slot.quantity >= itemDefinition.maxStack) {
            continue;
        }
        const int moved = std::min(remaining, itemDefinition.maxStack - slot.quantity);
        slot.quantity += moved;
        slot.quality = std::max(slot.quality, clampedQuality);
        remaining -= moved;
    }
    for (InventorySlot& slot : slots_) {
        if (remaining <= 0) {
            break;
        }
        if (!slot.empty()) {
            continue;
        }
        const int moved = std::min(remaining, itemDefinition.maxStack);
        slot = InventorySlot{
            item,
            moved,
            itemDefinition.maxDurability,
            clampedQuality,
        };
        remaining -= moved;
    }
    return remaining == 0;
}

bool Inventory::removeItem(ItemId item, int quantity) {
    if (item == ItemId::Empty || quantity <= 0 || itemCount(item) < quantity) {
        return false;
    }
    int remaining = quantity;
    for (auto iterator = slots_.rbegin(); iterator != slots_.rend() && remaining > 0; ++iterator) {
        InventorySlot& slot = *iterator;
        if (slot.item != item) {
            continue;
        }
        const int removed = std::min(slot.quantity, remaining);
        slot.quantity -= removed;
        remaining -= removed;
        if (slot.quantity <= 0) {
            slot = {};
        }
    }
    compactSlots();
    return true;
}

void Inventory::addWood(int amount) {
    static_cast<void>(addItem(ItemId::Wood, amount));
}

void Inventory::addStone(int amount) {
    static_cast<void>(addItem(ItemId::Stone, amount));
}

void Inventory::addWater(int amount) {
    static_cast<void>(addItem(ItemId::CleanWater, amount));
}

void Inventory::addFiber(int amount) {
    static_cast<void>(addItem(ItemId::Fiber, amount));
}

void Inventory::addMetal(int amount) {
    static_cast<void>(addItem(ItemId::Metal, amount));
}

bool Inventory::spendWood(int amount) {
    return removeItem(ItemId::Wood, amount);
}

bool Inventory::spendStone(int amount) {
    return removeItem(ItemId::Stone, amount);
}

bool Inventory::spendWater(int amount) {
    return removeItem(ItemId::CleanWater, amount);
}

bool Inventory::spendFiber(int amount) {
    return removeItem(ItemId::Fiber, amount);
}

bool Inventory::spendMetal(int amount) {
    return removeItem(ItemId::Metal, amount);
}

bool Inventory::spendResources(int wood, int stone, int fiber, int metal) {
    if (!canAfford(wood, stone, fiber, metal)) {
        return false;
    }
    static_cast<void>(spendWood(wood));
    static_cast<void>(spendStone(stone));
    static_cast<void>(spendFiber(fiber));
    static_cast<void>(spendMetal(metal));
    return true;
}

bool Inventory::unlockTool(Tool tool) {
    const std::size_t slot = slotForTool(tool);
    if (slot >= tools_.size() || tools_[slot] == tool) {
        return false;
    }
    if (!addItem(itemForTool(tool), 1)) {
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
    return slot < tools_.size() ? toolDisplayName(tools_[slot]) : std::string_view{};
}

bool Inventory::hasTool(Tool tool) const {
    return std::find(tools_.begin(), tools_.end(), tool) != tools_.end();
}

bool Inventory::hasItem(ItemId item, int quantity) const {
    return itemCount(item) >= quantity;
}

int Inventory::itemCount(ItemId item) const {
    int total = 0;
    for (const InventorySlot& slot : slots_) {
        if (slot.item == item) {
            total += slot.quantity;
        }
    }
    return total;
}

bool Inventory::canAfford(int wood, int stone, int fiber, int metal) const {
    if (wood < 0 || stone < 0 || fiber < 0 || metal < 0) {
        return false;
    }
    return itemCount(ItemId::Wood) >= wood
        && itemCount(ItemId::Stone) >= stone
        && itemCount(ItemId::Fiber) >= fiber
        && itemCount(ItemId::Metal) >= metal;
}

bool Inventory::canAdd(ItemId item, int quantity) const {
    return canFitAdditional(item, quantity);
}

int Inventory::wood() const {
    return itemCount(ItemId::Wood);
}

int Inventory::stone() const {
    return itemCount(ItemId::Stone);
}

int Inventory::water() const {
    return itemCount(ItemId::CleanWater);
}

int Inventory::fiber() const {
    return itemCount(ItemId::Fiber);
}

int Inventory::metal() const {
    return itemCount(ItemId::Metal);
}

float Inventory::totalWeight() const {
    float total = 0.0F;
    for (const InventorySlot& slot : slots_) {
        if (!slot.empty()) {
            total += definition(slot.item).weight * static_cast<float>(slot.quantity);
        }
    }
    return total;
}

float Inventory::totalVolume() const {
    float total = 0.0F;
    for (const InventorySlot& slot : slots_) {
        if (!slot.empty()) {
            total += definition(slot.item).volume * static_cast<float>(slot.quantity);
        }
    }
    return total;
}

InventoryCapacity Inventory::capacity() const {
    return capacity_;
}

const std::array<InventorySlot, Inventory::slotCount>& Inventory::slots() const {
    return slots_;
}

std::vector<InventorySlot> Inventory::filteredSlots(InventoryFilter filter) const {
    std::vector<InventorySlot> result;
    result.reserve(slots_.size());
    for (const InventorySlot& slot : slots_) {
        if (!slot.empty() && categoryMatches(definition(slot.item).category, filter)) {
            result.push_back(slot);
        }
    }
    return result;
}

void Inventory::sort(InventorySortMode mode) {
    std::stable_sort(slots_.begin(), slots_.end(), [mode](const InventorySlot& left, const InventorySlot& right) {
        if (left.empty() || right.empty()) {
            return !left.empty() && right.empty();
        }
        const ItemDefinition& leftDefinition = Inventory::definition(left.item);
        const ItemDefinition& rightDefinition = Inventory::definition(right.item);
        switch (mode) {
        case InventorySortMode::Category:
            if (leftDefinition.category != rightDefinition.category) {
                return categoryRank(leftDefinition.category) < categoryRank(rightDefinition.category);
            }
            return leftDefinition.displayName < rightDefinition.displayName;
        case InventorySortMode::Name:
            return leftDefinition.displayName < rightDefinition.displayName;
        case InventorySortMode::Quantity:
            if (left.quantity != right.quantity) {
                return left.quantity > right.quantity;
            }
            return leftDefinition.displayName < rightDefinition.displayName;
        }
        return false;
    });
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
        wood(),
        stone(),
        water(),
        fiber(),
        metal(),
    };
}

void Inventory::applySnapshot(const InventorySnapshot& snapshot) {
    slots_ = {};
    tools_.fill(Tool::Empty);
    if (snapshot.tools[0]) {
        tools_[slotForTool(Tool::Axe)] = Tool::Axe;
        static_cast<void>(addItem(ItemId::Axe, 1));
    }
    if (snapshot.tools[1]) {
        tools_[slotForTool(Tool::Knife)] = Tool::Knife;
        static_cast<void>(addItem(ItemId::Knife, 1));
    }
    if (snapshot.tools[2]) {
        tools_[slotForTool(Tool::Pickaxe)] = Tool::Pickaxe;
        static_cast<void>(addItem(ItemId::Pickaxe, 1));
    }
    if (snapshot.tools[3]) {
        tools_[slotForTool(Tool::Torch)] = Tool::Torch;
        static_cast<void>(addItem(ItemId::Torch, 1));
    }
    selectedSlot_ = snapshot.selectedSlot < hotbarSize ? snapshot.selectedSlot : 0;
    static_cast<void>(addItem(ItemId::Wood, std::max(snapshot.wood, 0)));
    static_cast<void>(addItem(ItemId::Stone, std::max(snapshot.stone, 0)));
    static_cast<void>(addItem(ItemId::CleanWater, std::max(snapshot.water, 0)));
    static_cast<void>(addItem(ItemId::Fiber, std::max(snapshot.fiber, 0)));
    static_cast<void>(addItem(ItemId::Metal, std::max(snapshot.metal, 0)));
}

const ItemDefinition& Inventory::definition(ItemId item) {
    const auto iterator = std::find_if(itemDefinitions.begin(), itemDefinitions.end(), [item](const ItemDefinition& definition) {
        return definition.id == item;
    });
    return iterator != itemDefinitions.end() ? *iterator : itemDefinitions.front();
}

ItemId Inventory::itemForTool(Tool tool) {
    switch (tool) {
    case Tool::Axe:
        return ItemId::Axe;
    case Tool::Knife:
        return ItemId::Knife;
    case Tool::Pickaxe:
        return ItemId::Pickaxe;
    case Tool::Torch:
        return ItemId::Torch;
    case Tool::Empty:
        return ItemId::Empty;
    }
    return ItemId::Empty;
}

std::string_view Inventory::toolDisplayName(Tool tool) {
    return definition(itemForTool(tool)).displayName;
}

bool Inventory::canFitAdditional(ItemId item, int quantity) const {
    if (item == ItemId::Empty || quantity <= 0) {
        return false;
    }
    const ItemDefinition& itemDefinition = definition(item);
    const float addedWeight = itemDefinition.weight * static_cast<float>(quantity);
    const float addedVolume = itemDefinition.volume * static_cast<float>(quantity);
    if (totalWeight() + addedWeight > capacity_.maxWeight || totalVolume() + addedVolume > capacity_.maxVolume) {
        return false;
    }

    int remaining = quantity;
    for (const InventorySlot& slot : slots_) {
        if (slot.item == item) {
            remaining -= std::max(itemDefinition.maxStack - slot.quantity, 0);
        }
        if (slot.empty()) {
            remaining -= itemDefinition.maxStack;
        }
        if (remaining <= 0) {
            return true;
        }
    }
    return false;
}

void Inventory::compactSlots() {
    std::stable_sort(slots_.begin(), slots_.end(), [](const InventorySlot& left, const InventorySlot& right) {
        return !left.empty() && right.empty();
    });
}

} // namespace pcolonist
