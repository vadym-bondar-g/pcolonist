#include "pcolonist/render/Camera.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace pcolonist {

Camera::Camera(glm::vec3 position)
    : position_(position) {
    updateVectors();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

const glm::vec3& Camera::position() const {
    return position_;
}

const glm::vec3& Camera::front() const {
    return front_;
}

glm::vec3 Camera::horizontalFront() const {
    return glm::normalize(glm::vec3{front_.x, 0.0F, front_.z});
}

glm::vec3 Camera::horizontalRight() const {
    return glm::normalize(glm::vec3{right_.x, 0.0F, right_.z});
}

void Camera::setPosition(glm::vec3 position) {
    position_ = position;
}

void Camera::move(CameraMovement direction, float deltaTime) {
    const float velocity = movementSpeed_ * deltaTime;

    switch (direction) {
    case CameraMovement::Forward:
        position_ += front_ * velocity;
        break;
    case CameraMovement::Backward:
        position_ -= front_ * velocity;
        break;
    case CameraMovement::Left:
        position_ -= right_ * velocity;
        break;
    case CameraMovement::Right:
        position_ += right_ * velocity;
        break;
    case CameraMovement::Up:
        position_ += worldUp_ * velocity;
        break;
    case CameraMovement::Down:
        position_ -= worldUp_ * velocity;
        break;
    }
}

void Camera::look(float xOffset, float yOffset) {
    yaw_ += xOffset * mouseSensitivity_;
    pitch_ += yOffset * mouseSensitivity_;
    pitch_ = std::clamp(pitch_, -89.0F, 89.0F);
    updateVectors();
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
    front.y = std::sin(glm::radians(pitch_));
    front.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));

    front_ = glm::normalize(front);
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    up_ = glm::normalize(glm::cross(right_, front_));
}

} // namespace pcolonist
