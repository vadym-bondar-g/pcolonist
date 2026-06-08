#pragma once

namespace pcolonist {

class Camera;
class Shader;
class WeatherSystem;

class Skybox {
public:
    Skybox();
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    void render(Shader& shader, const Camera& camera, const WeatherSystem& weather, float aspectRatio) const;

private:
    unsigned int vertexArray_ = 0;
};

} // namespace pcolonist
