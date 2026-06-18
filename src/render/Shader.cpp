#include "pcolonist/render/Shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot open shader: " + path.string());
    }

    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

unsigned int compileShader(unsigned int type, const std::string& source, const std::filesystem::path& path) {
    const unsigned int shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return shader;
    }

    int logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(logLength), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("Shader compilation failed (" + path.string() + "):\n" + log);
}

} // namespace

namespace pcolonist {

Shader::Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath) {
    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, readFile(vertexPath), vertexPath);
    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath), fragmentPath);

    id_ = glCreateProgram();
    glAttachShader(id_, vertexShader);
    glAttachShader(id_, fragmentShader);
    glLinkProgram(id_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &success);
    if (success == GL_TRUE) {
        return;
    }

    int logLength = 0;
    glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(logLength), '\0');
    glGetProgramInfoLog(id_, logLength, nullptr, log.data());
    glDeleteProgram(id_);
    id_ = 0;
    throw std::runtime_error("Shader program linking failed:\n" + log);
}

Shader::~Shader() {
    if (id_ != 0) {
        glDeleteProgram(id_);
    }
}

void Shader::use() const {
    glUseProgram(id_);
}

void Shader::setFloat(std::string_view name, float value) const {
    glUniform1f(uniformLocation(name), value);
}

void Shader::setInt(std::string_view name, int value) const {
    glUniform1i(uniformLocation(name), value);
}

void Shader::setMat4(std::string_view name, const glm::mat4& value) const {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(std::string_view name, const glm::vec2& value) const {
    glUniform2fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec3(std::string_view name, const glm::vec3& value) const {
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec4(std::string_view name, const glm::vec4& value) const {
    glUniform4fv(uniformLocation(name), 1, glm::value_ptr(value));
}

bool Shader::validate() const {
    glValidateProgram(id_);
    int valid = 0;
    glGetProgramiv(id_, GL_VALIDATE_STATUS, &valid);
    return valid == GL_TRUE;
}

int Shader::uniformLocation(std::string_view name) const {
    const std::string key(name);
    if (const auto iterator = uniforms_.find(key); iterator != uniforms_.end()) {
        return iterator->second;
    }
    return uniforms_.emplace(key, glGetUniformLocation(id_, key.c_str())).first->second;
}

} // namespace pcolonist
