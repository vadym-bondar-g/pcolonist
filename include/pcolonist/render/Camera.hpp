#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace pcolonist {

enum class CameraMovement {
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down,
};

class Camera {
public:
    explicit Camera(glm::vec3 position = {0.0F, 1.0F, 3.0F});

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] const glm::vec3& position() const;
    [[nodiscard]] const glm::vec3& front() const;
    [[nodiscard]] glm::vec3 horizontalFront() const;
    [[nodiscard]] glm::vec3 horizontalRight() const;

    void setPosition(glm::vec3 position);
    void move(CameraMovement direction, float deltaTime);
    void look(float xOffset, float yOffset);

private:
    void updateVectors();

    glm::vec3 position_;
    glm::vec3 front_ = {0.0F, 0.0F, -1.0F};
    glm::vec3 up_ = {0.0F, 1.0F, 0.0F};
    glm::vec3 right_ = {1.0F, 0.0F, 0.0F};
    glm::vec3 worldUp_ = {0.0F, 1.0F, 0.0F};
    float yaw_ = -90.0F;
    float pitch_ = 0.0F;
    float movementSpeed_ = 4.0F;
    float mouseSensitivity_ = 0.1F;
};

} // namespace pcolonist
