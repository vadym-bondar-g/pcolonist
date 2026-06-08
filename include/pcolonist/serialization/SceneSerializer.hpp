#pragma once

#include <string>
#include <string_view>

namespace pcolonist {

class Registry;

class SceneSerializer {
public:
    [[nodiscard]] static std::string serialize(Registry& registry);
    static void deserialize(std::string_view data, Registry& registry);
};

} // namespace pcolonist
