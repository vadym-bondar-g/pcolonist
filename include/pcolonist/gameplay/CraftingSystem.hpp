#pragma once

#include "pcolonist/gameplay/Inventory.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace pcolonist {

struct CraftingRecipe {
    std::string_view name;
    Tool unlocks = Tool::Empty;
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int metal = 0;
    bool requiresCraftStation = false;
};

struct CraftingResult {
    bool crafted = false;
    std::string message;
};

class CraftingSystem {
public:
    [[nodiscard]] CraftingResult craftNextAvailable(Inventory& inventory, bool nearCraftStation) const;
    [[nodiscard]] std::string nextHint(const Inventory& inventory, bool nearCraftStation) const;
    [[nodiscard]] const std::vector<CraftingRecipe>& recipes() const;

private:
    std::vector<CraftingRecipe> recipes_{
        {"КАМЕННЫЙ НОЖ", Tool::Knife, 1, 2, 1, 0, false},
        {"ФАКЕЛ", Tool::Torch, 2, 0, 2, 0, false},
        {"КИРКА", Tool::Pickaxe, 2, 5, 2, 1, true},
    };
};

} // namespace pcolonist
