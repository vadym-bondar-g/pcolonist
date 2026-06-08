#pragma once

#include "pcolonist/render/Mesh.hpp"

#include <glm/vec3.hpp>

namespace pcolonist {

class MeshFactory {
public:
    static Mesh cube(glm::vec3 color);
    static Mesh plane(float size, glm::vec3 color);
};

} // namespace pcolonist
