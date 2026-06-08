#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;

out vec3 vertexColor;
out vec3 worldPosition;
out vec3 worldNormal;
out vec4 lightSpacePosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float water;
uniform float time;

void main() {
    vec3 localPosition = position;
    vec3 localNormal = normal;
    if (water > 0.5) {
        localPosition.y += sin(position.x * 0.08 + time) * 0.12;
        localPosition.y += cos(position.z * 0.1 + time * 0.7) * 0.08;
        float slopeX = cos(position.x * 0.08 + time) * 0.0096;
        float slopeZ = -sin(position.z * 0.1 + time * 0.7) * 0.008;
        localNormal = normalize(vec3(-slopeX, 1.0, -slopeZ));
    }
    vec4 world = model * vec4(localPosition, 1.0);
    vertexColor = color;
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(transpose(inverse(model))) * localNormal);
    lightSpacePosition = lightSpaceMatrix * world;
    gl_Position = projection * view * world;
}
