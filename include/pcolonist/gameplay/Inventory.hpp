#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace pcolonist {

enum class Tool {
    Axe,
    Knife,
    Pickaxe,
    Torch,
    Empty,
};

enum class ItemId {
    Empty,
    Wood,
    Stone,
    CleanWater,
    Fiber,
    Metal,
    Axe,
    Knife,
    Pickaxe,
    Torch,
};

enum class ItemCategory {
    Resource,
    Consumable,
    Tool,
    Equipment,
    Quest,
};

enum class ItemRarity {
    Common,
    Uncommon,
    Rare,
    Epic,
};

struct ItemDefinition {
    ItemId id = ItemId::Empty;
    std::string_view displayName;
    ItemCategory category = ItemCategory::Resource;
    ItemRarity rarity = ItemRarity::Common;
    int maxStack = 1;
    float weight = 0.0F;
    float volume = 0.0F;
    float maxDurability = 0.0F;
};

struct InventorySlot {
    ItemId item = ItemId::Empty;
    int quantity = 0;
    float durability = 0.0F;
    float quality = 1.0F;

    [[nodiscard]] bool empty() const {
        return item == ItemId::Empty || quantity <= 0;
    }
};

struct InventoryCapacity {
    float maxWeight = 45.0F;
    float maxVolume = 80.0F;
};

enum class InventorySortMode {
    Category,
    Name,
    Quantity,
};

enum class InventoryFilter {
    All,
    Resources,
    Consumables,
    Tools,
};

struct InventorySnapshot {
    std::array<bool, 4> tools{};
    std::size_t selectedSlot = 0;
    int wood = 0;
    int stone = 0;
    int water = 0;
    int fiber = 0;
    int metal = 0;
};

class Inventory {
public:
    static constexpr std::size_t hotbarSize = 5;
    static constexpr std::size_t slotCount = 24;

    Inventory();

    void select(std::size_t slot);
    [[nodiscard]] bool addItem(ItemId item, int quantity = 1, float quality = 1.0F);
    [[nodiscard]] bool removeItem(ItemId item, int quantity = 1);
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
    [[nodiscard]] bool hasItem(ItemId item, int quantity = 1) const;
    [[nodiscard]] int itemCount(ItemId item) const;
    [[nodiscard]] bool canAfford(int wood, int stone, int fiber, int metal) const;
    [[nodiscard]] bool canAdd(ItemId item, int quantity = 1) const;
    [[nodiscard]] int wood() const;
    [[nodiscard]] int stone() const;
    [[nodiscard]] int water() const;
    [[nodiscard]] int fiber() const;
    [[nodiscard]] int metal() const;
    [[nodiscard]] float totalWeight() const;
    [[nodiscard]] float totalVolume() const;
    [[nodiscard]] InventoryCapacity capacity() const;
    [[nodiscard]] const std::array<InventorySlot, slotCount>& slots() const;
    [[nodiscard]] std::vector<InventorySlot> filteredSlots(InventoryFilter filter) const;
    void sort(InventorySortMode mode);
    [[nodiscard]] InventorySnapshot snapshot() const;
    void applySnapshot(const InventorySnapshot& snapshot);

    [[nodiscard]] static const ItemDefinition& definition(ItemId item);

private:
    [[nodiscard]] static ItemId itemForTool(Tool tool);
    [[nodiscard]] static std::string_view toolDisplayName(Tool tool);
    [[nodiscard]] bool canFitAdditional(ItemId item, int quantity) const;
    void compactSlots();

    std::array<InventorySlot, slotCount> slots_{};
    std::array<Tool, hotbarSize> tools_{};
    std::size_t selectedSlot_ = 0;
    InventoryCapacity capacity_;
};

} // namespace pcolonist
