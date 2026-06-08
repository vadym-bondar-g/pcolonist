#include "pcolonist/render/Skybox.hpp"

#include "pcolonist/render/Camera.hpp"
#include "pcolonist/render/Shader.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace pcolonist {

Skybox::Skybox() {
    glGenVertexArrays(1, &vertexArray_);
}

Skybox::~Skybox() {
    glDeleteVertexArrays(1, &vertexArray_);
}

void Skybox::render(Shader& shader, const Camera& camera, const WeatherSystem& weather, float aspectRatio) const {
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    const glm::mat4 projection = glm::perspective(glm::radians(70.0F), aspectRatio, 0.1F, 250.0F);
    const glm::mat4 rotationView = glm::mat4(glm::mat3(camera.viewMatrix()));
    shader.use();
    shader.setMat4("inverseProjection", glm::inverse(projection));
    shader.setMat4("inverseView", glm::inverse(rotationView));
    shader.setVec3("horizonColor", weather.horizonColor());
    shader.setVec3("zenithColor", weather.skyColor());
    shader.setVec3("sunDirection", weather.sunDirection());
    shader.setVec3("moonDirection", weather.moonDirection());
    shader.setVec3("moonColor", weather.moonColor());
    shader.setFloat("cloudiness", weather.cloudiness());
    shader.setFloat("daylight", weather.daylight());
    shader.setFloat("nightFactor", weather.nightFactor());
    shader.setFloat("time", weather.time());
    glBindVertexArray(vertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

} // namespace pcolonist
