#include "pcolonist/gameplay/CraftingSystem.hpp"

#include <sstream>

namespace {

std::string recipeCostText(const pcolonist::CraftingRecipe& recipe) {
    std::ostringstream stream;
    bool first = true;
    const auto add = [&stream, &first](std::string_view name, int value) {
        if (value <= 0) {
            return;
        }
        if (!first) {
            stream << ' ';
        }
        stream << name << ':' << value;
        first = false;
    };
    add("Д", recipe.wood);
    add("К", recipe.stone);
    add("ВЛ", recipe.fiber);
    add("М", recipe.metal);
    return stream.str();
}

} // namespace

namespace pcolonist {

CraftingResult CraftingSystem::craftNextAvailable(Inventory& inventory, bool nearCraftStation) const {
    for (const CraftingRecipe& recipe : recipes_) {
        if (inventory.hasTool(recipe.unlocks)) {
            continue;
        }
        if (recipe.requiresCraftStation && !nearCraftStation) {
            return {false, "Нужен лагерь или мастерская"};
        }
        if (!inventory.canAfford(recipe.wood, recipe.stone, recipe.fiber, recipe.metal)) {
            return {false, "Не хватает: " + recipeCostText(recipe)};
        }
        if (inventory.spendResources(recipe.wood, recipe.stone, recipe.fiber, recipe.metal)
            && inventory.unlockTool(recipe.unlocks)) {
            return {true, "Создано: " + std::string(recipe.name)};
        }
    }
    return {false, "Все базовые рецепты открыты"};
}

std::string CraftingSystem::nextHint(const Inventory& inventory, bool nearCraftStation) const {
    for (const CraftingRecipe& recipe : recipes_) {
        if (inventory.hasTool(recipe.unlocks)) {
            continue;
        }
        if (recipe.requiresCraftStation && !nearCraftStation) {
            return "C: " + std::string(recipe.name) + " у лагеря";
        }
        return "C: " + std::string(recipe.name) + " " + recipeCostText(recipe);
    }
    return "КРАФТ: ВСЕ БАЗОВОЕ СОЗДАНО";
}

const std::vector<CraftingRecipe>& CraftingSystem::recipes() const {
    return recipes_;
}

} // namespace pcolonist
