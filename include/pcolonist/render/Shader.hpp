#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pcolonist {

class Shader {
public:
    Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setFloat(std::string_view name, float value) const;
    void setInt(std::string_view name, int value) const;
    void setMat4(std::string_view name, const glm::mat4& value) const;
    void setVec3(std::string_view name, const glm::vec3& value) const;
    void setVec4(std::string_view name, const glm::vec4& value) const;
    [[nodiscard]] bool validate() const;

private:
    int uniformLocation(std::string_view name) const;

    unsigned int id_ = 0;
    mutable std::unordered_map<std::string, int> uniforms_;
};

} // namespace pcolonist
