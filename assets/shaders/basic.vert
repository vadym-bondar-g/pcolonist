#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 textureCoordinate;
layout (location = 4) in mat4 instanceModel;

out vec3 vertexColor;
out vec3 worldPosition;
out vec3 worldNormal;
out vec4 lightSpacePositions[2];
out vec2 uv;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrices[2];
uniform float water;
uniform float time;

float waveHeight(vec2 worldXZ) {
    return sin(dot(vec2(0.940376, 0.340136), worldXZ) * 0.18 + time * 1.05) * 0.16
        + sin(dot(vec2(-0.28, 0.96), worldXZ) * 0.31 + time * 0.72) * 0.09
        + sin(dot(vec2(0.707107, -0.707107), worldXZ) * 0.57 + time * 1.42) * 0.045
        + sin(dot(vec2(-0.819232, -0.573462), worldXZ) * 0.93 + time * 1.86) * 0.022
        + sin(dot(vec2(0.196116, -0.980581), worldXZ) * 1.41 + time * 2.24) * 0.012;
}

vec3 waveNormal(vec2 worldXZ) {
    vec2 gradient =
        vec2(0.940376, 0.340136) * cos(dot(vec2(0.940376, 0.340136), worldXZ) * 0.18 + time * 1.05) * 0.16 * 0.18
        + vec2(-0.28, 0.96) * cos(dot(vec2(-0.28, 0.96), worldXZ) * 0.31 + time * 0.72) * 0.09 * 0.31
        + vec2(0.707107, -0.707107) * cos(dot(vec2(0.707107, -0.707107), worldXZ) * 0.57 + time * 1.42) * 0.045 * 0.57
        + vec2(-0.819232, -0.573462) * cos(dot(vec2(-0.819232, -0.573462), worldXZ) * 0.93 + time * 1.86) * 0.022 * 0.93
        + vec2(0.196116, -0.980581) * cos(dot(vec2(0.196116, -0.980581), worldXZ) * 1.41 + time * 2.24) * 0.012 * 1.41;
    return normalize(vec3(-gradient.x, 1.0, -gradient.y));
}

void main() {
    vec4 world = instanceModel * vec4(position, 1.0);
    vec3 transformedNormal = normalize(mat3(transpose(inverse(instanceModel))) * normal);
    if (water > 0.5) {
        world.y += waveHeight(world.xz);
        transformedNormal = waveNormal(world.xz);
    }
    vertexColor = color;
    worldPosition = world.xyz;
    worldNormal = transformedNormal;
    lightSpacePositions[0] = lightSpaceMatrices[0] * world;
    lightSpacePositions[1] = lightSpaceMatrices[1] * world;
    uv = vec2(textureCoordinate.x, 1.0 - textureCoordinate.y);
    gl_Position = projection * view * world;
}
