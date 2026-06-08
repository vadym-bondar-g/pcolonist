#include "pcolonist/render/ShaderLibrary.hpp"

#include <stdexcept>
#include <utility>

namespace pcolonist {

ShaderLibrary::ShaderLibrary(std::filesystem::path shaderDirectory)
    : directory_(std::move(shaderDirectory)),
      definitions_{
          {"scene", "basic.vert", "basic.frag"},
          {"sky", "sky.vert", "sky.frag"},
          {"shadow", "shadow.vert", "shadow.frag"},
          {"post", "post.vert", "post.frag"},
      } {}

void ShaderLibrary::preload() {
    for (const Definition& definition : definitions_) {
        auto shader = std::make_unique<Shader>(directory_ / definition.vertex, directory_ / definition.fragment);
        shader->use();
        if (!shader->validate()) {
            throw std::runtime_error("Shader validation failed: " + definition.name);
        }
        shaders_.insert_or_assign(definition.name, std::move(shader));
    }
}

Shader& ShaderLibrary::get(const std::string& name) {
    const auto iterator = shaders_.find(name);
    if (iterator == shaders_.end()) {
        throw std::out_of_range("Shader is not precompiled: " + name);
    }
    return *iterator->second;
}

} // namespace pcolonist
