#include "pcolonist/gameplay/DiscoverySystem.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

std::string extractString(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t colon = object.find(':', keyPosition + pattern.size());
    const std::size_t firstQuote = object.find('"', colon + 1);
    if (colon == std::string_view::npos || firstQuote == std::string_view::npos) {
        return {};
    }
    std::size_t secondQuote = firstQuote + 1;
    while (secondQuote < object.size()) {
        if (object[secondQuote] == '"' && object[secondQuote - 1] != '\\') {
            return std::string(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        }
        ++secondQuote;
    }
    return {};
}

std::vector<float> extractNumberArray(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t open = object.find('[', keyPosition + pattern.size());
    const std::size_t close = object.find(']', open + 1);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return {};
    }
    std::vector<float> values;
    std::size_t cursor = open + 1;
    while (cursor < close) {
        while (cursor < close && (std::isspace(static_cast<unsigned char>(object[cursor])) || object[cursor] == ',')) {
            ++cursor;
        }
        if (cursor >= close) {
            break;
        }
        std::size_t end = cursor;
        while (end < close && object[end] != ',') {
            ++end;
        }
        values.push_back(std::stof(std::string(object.substr(cursor, end - cursor))));
        cursor = end + 1;
    }
    return values;
}

std::vector<std::string> extractStringArray(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t open = object.find('[', keyPosition + pattern.size());
    const std::size_t close = object.find(']', open + 1);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return {};
    }
    std::vector<std::string> values;
    std::size_t cursor = open + 1;
    while (cursor < close) {
        const std::size_t firstQuote = object.find('"', cursor);
        if (firstQuote == std::string_view::npos || firstQuote >= close) {
            break;
        }
        const std::size_t secondQuote = object.find('"', firstQuote + 1);
        if (secondQuote == std::string_view::npos || secondQuote >= close) {
            break;
        }
        values.emplace_back(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        cursor = secondQuote + 1;
    }
    return values;
}

bool isSecretBiome(const std::string& biome) {
    return biome.find("Тай") != std::string::npos || biome.find("тай") != std::string::npos;
}

} // namespace

namespace pcolonist {

void DiscoverySystem::loadLocations(const AssetSystem& assets, const std::filesystem::path& relativePath) {
    locations_.clear();

    std::string content;
    try {
        content = assets.readText(relativePath);
    } catch (const std::exception&) {
        recount();
        return;
    }

    int depth = 0;
    std::size_t objectStart = std::string::npos;
    for (std::size_t index = 0; index < content.size(); ++index) {
        if (content[index] == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (content[index] == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                const std::string_view object{content.data() + objectStart, index - objectStart + 1};
                Location location;
                location.name = extractString(object, "name");
                location.biome = extractString(object, "biome");
                const auto position = extractNumberArray(object, "position");
                if (position.size() >= 3) {
                    location.position = {position[0], position[1], position[2]};
                }
                location.storyClues = extractStringArray(object, "story_clues");
                location.requiredTools = extractStringArray(object, "required_tools");
                if (!location.name.empty()) {
                    locations_.push_back(std::move(location));
                }
                objectStart = std::string::npos;
            }
        }
    }
    recount();
}

void DiscoverySystem::update(glm::vec3 playerPosition, const Inventory& inventory) {
    Location* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (Location& location : locations_) {
        const float distance = glm::length(glm::vec2{playerPosition.x - location.position.x, playerPosition.z - location.position.z});
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = &location;
        }
    }

    currentLocation_ = nearestDistance <= 34.0F ? nearest : nullptr;
    status_.blocked = false;
    if (currentLocation_ == nullptr) {
        status_.currentLocation = "Неизведанная местность";
        return;
    }

    Location& location = *nearest;
    status_.currentLocation = location.name;
    if (!hasRequiredTools(location, inventory)) {
        status_.blocked = true;
        status_.lastMessage = "Нужен инструмент: " + missingToolLabel(location, inventory);
        return;
    }

    if (nearestDistance <= 22.0F && !location.discovered) {
        location.discovered = true;
        status_.lastMessage = "Открыто: " + location.name;
        if (!location.storyClues.empty() && !location.clueClaimed) {
            location.clueClaimed = true;
            status_.clueMessage = "Улика: " + location.storyClues.front();
        }
        recount();
    } else if (location.discovered && !location.storyClues.empty()) {
        status_.clueMessage = "Улика: " + location.storyClues.front();
    }
}

const DiscoveryStatus& DiscoverySystem::status() const {
    return status_;
}

bool DiscoverySystem::hasDiscoveredCurrentLocation() const {
    return currentLocation_ != nullptr && currentLocation_->discovered;
}

DiscoverySnapshot DiscoverySystem::snapshot() const {
    DiscoverySnapshot snapshot;
    snapshot.discovered.reserve(locations_.size());
    snapshot.clueClaimed.reserve(locations_.size());
    for (const Location& location : locations_) {
        snapshot.discovered.push_back(location.discovered);
        snapshot.clueClaimed.push_back(location.clueClaimed);
    }
    return snapshot;
}

void DiscoverySystem::applySnapshot(const DiscoverySnapshot& snapshot) {
    for (std::size_t index = 0; index < locations_.size(); ++index) {
        locations_[index].discovered = index < snapshot.discovered.size() && snapshot.discovered[index];
        locations_[index].clueClaimed = index < snapshot.clueClaimed.size() && snapshot.clueClaimed[index];
    }
    currentLocation_ = nullptr;
    status_.currentLocation = "Неизведанная местность";
    status_.lastMessage = "Сохранение загружено";
    status_.clueMessage = "Следов пока нет";
    status_.blocked = false;
    recount();
}

bool DiscoverySystem::hasRequiredTools(const Location& location, const Inventory& inventory) const {
    for (const std::string& tool : location.requiredTools) {
        if (tool.find("топор") != std::string::npos && !inventory.hasTool(Tool::Axe)) {
            return false;
        }
        if (tool.find("нож") != std::string::npos && !inventory.hasTool(Tool::Knife)) {
            return false;
        }
        if (tool.find("кир") != std::string::npos && !inventory.hasTool(Tool::Pickaxe)) {
            return false;
        }
        if (tool.find("факел") != std::string::npos && !inventory.hasTool(Tool::Torch)) {
            return false;
        }
        if (tool.find("лод") != std::string::npos) {
            return false;
        }
    }
    return true;
}

std::string DiscoverySystem::missingToolLabel(const Location& location, const Inventory& inventory) const {
    for (const std::string& tool : location.requiredTools) {
        if (tool.find("топор") != std::string::npos && !inventory.hasTool(Tool::Axe)) {
            return "ТОПОР";
        }
        if (tool.find("нож") != std::string::npos && !inventory.hasTool(Tool::Knife)) {
            return "НОЖ";
        }
        if (tool.find("кир") != std::string::npos && !inventory.hasTool(Tool::Pickaxe)) {
            return "КИРКА";
        }
        if (tool.find("факел") != std::string::npos && !inventory.hasTool(Tool::Torch)) {
            return "ФАКЕЛ";
        }
        if (tool.find("лод") != std::string::npos) {
            return "ЛОДКА";
        }
    }
    return "НЕИЗВЕСТНО";
}

void DiscoverySystem::recount() {
    status_.totalLocations = static_cast<int>(locations_.size());
    status_.discoveredLocations = 0;
    status_.storyClues = 0;
    status_.totalStoryClues = 0;
    status_.secretsFound = 0;
    status_.totalSecrets = 0;
    for (const Location& location : locations_) {
        if (location.discovered) {
            ++status_.discoveredLocations;
        }
        if (!location.storyClues.empty()) {
            ++status_.totalStoryClues;
            if (location.clueClaimed) {
                ++status_.storyClues;
            }
        }
        if (isSecretBiome(location.biome) || !location.requiredTools.empty()) {
            ++status_.totalSecrets;
            if (location.discovered) {
                ++status_.secretsFound;
            }
        }
    }
}

} // namespace pcolonist
