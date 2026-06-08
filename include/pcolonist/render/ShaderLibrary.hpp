#pragma once

#include "pcolonist/render/Shader.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace pcolonist {

class ShaderLibrary {
public:
    explicit ShaderLibrary(std::filesystem::path shaderDirectory);

    void preload();
    [[nodiscard]] Shader& get(const std::string& name);

private:
    struct Definition {
        std::string name;
        std::filesystem::path vertex;
        std::filesystem::path fragment;
    };

    std::filesystem::path directory_;
    std::vector<Definition> definitions_;
    std::unordered_map<std::string, std::unique_ptr<Shader>> shaders_;
};

} // namespace pcolonist
