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
uniform int waterKind;
uniform vec2 waterFlowDirection;
uniform float lava;
uniform float fire;
uniform float smoke;
uniform float time;

float waveHeight(vec2 worldXZ) {
    float height = sin(dot(vec2(0.940376, 0.340136), worldXZ) * 0.18 + time * 1.05) * 0.16
        + sin(dot(vec2(-0.28, 0.96), worldXZ) * 0.31 + time * 0.72) * 0.09
        + sin(dot(vec2(0.707107, -0.707107), worldXZ) * 0.57 + time * 1.42) * 0.045
        + sin(dot(vec2(-0.819232, -0.573462), worldXZ) * 0.93 + time * 1.86) * 0.022
        + sin(dot(vec2(0.196116, -0.980581), worldXZ) * 1.41 + time * 2.24) * 0.012;
    if (waterKind == 1) {
        vec2 flow = length(waterFlowDirection) > 0.001 ? normalize(waterFlowDirection) : vec2(0.0, 1.0);
        float current = sin(dot(flow, worldXZ) * 0.72 - time * 1.35) * 0.018;
        return height * 0.35 + current;
    }
    return height;
}

vec3 waveNormal(vec2 worldXZ) {
    vec2 gradient =
        vec2(0.940376, 0.340136) * cos(dot(vec2(0.940376, 0.340136), worldXZ) * 0.18 + time * 1.05) * 0.16 * 0.18
        + vec2(-0.28, 0.96) * cos(dot(vec2(-0.28, 0.96), worldXZ) * 0.31 + time * 0.72) * 0.09 * 0.31
        + vec2(0.707107, -0.707107) * cos(dot(vec2(0.707107, -0.707107), worldXZ) * 0.57 + time * 1.42) * 0.045 * 0.57
        + vec2(-0.819232, -0.573462) * cos(dot(vec2(-0.819232, -0.573462), worldXZ) * 0.93 + time * 1.86) * 0.022 * 0.93
        + vec2(0.196116, -0.980581) * cos(dot(vec2(0.196116, -0.980581), worldXZ) * 1.41 + time * 2.24) * 0.012 * 1.41;
    if (waterKind == 1) {
        vec2 flow = length(waterFlowDirection) > 0.001 ? normalize(waterFlowDirection) : vec2(0.0, 1.0);
        gradient = gradient * 0.35 + flow * cos(dot(flow, worldXZ) * 0.72 - time * 1.35) * 0.018 * 0.72;
    }
    return normalize(vec3(-gradient.x, 1.0, -gradient.y));
}

void main() {
    vec4 world = instanceModel * vec4(position, 1.0);
    vec3 transformedNormal = normalize(mat3(transpose(inverse(instanceModel))) * normal);
    if (water > 0.5) {
        world.y += waveHeight(world.xz);
        transformedNormal = waveNormal(world.xz);
    }
    if (lava > 0.5) {
        vec2 centeredUv = textureCoordinate - vec2(0.5);
        float radial = clamp(length(centeredUv) * 2.0, 0.0, 1.0);
        float liquid = 1.0 - smoothstep(0.52, 0.96, radial);
        float crustEdge = smoothstep(0.55, 0.98, radial);
        float bubble = sin(time * 1.8 + world.x * 0.62 + world.z * 0.41)
            + sin(time * 2.4 - world.x * 0.35 + world.z * 0.77)
            + sin(time * 3.2 + dot(world.xz, vec2(0.28, -0.91)));
        float lowRoll = sin(time * 0.62 + dot(world.xz, vec2(0.14, 0.22)));
        world.y += (bubble * 0.020 + lowRoll * 0.016) * liquid;
        world.y += sin(time * 1.1 + radial * 9.0) * 0.007 * (1.0 - crustEdge);
        transformedNormal = normalize(transformedNormal + vec3(centeredUv.x, 0.0, centeredUv.y) * (0.08 + 0.12 * liquid));
    }
    if (fire > 0.5) {
        float height = clamp(position.y / 1.18, 0.0, 1.0);
        float lift = smoothstep(0.08, 0.32, height);
        vec2 flameDirection = normalize(world.xz - instanceModel[3].xz + vec2(0.001));
        float flicker = sin(time * 8.7 + position.y * 9.0 + position.x * 13.0) * 0.035
            + sin(time * 14.3 + position.z * 11.0) * 0.022;
        world.xz += flameDirection * flicker * lift;
        world.y += (sin(time * 10.8 + position.x * 7.0 + position.z * 5.0) * 0.018 + height * height * 0.026) * lift;
    }
    if (smoke > 0.5) {
        float height = clamp(position.y / 1.18, 0.0, 1.0);
        float drift = sin(time * 1.35 + position.y * 3.2 + position.x * 5.1) * 0.065
            + sin(time * 0.92 + position.z * 4.3) * 0.04;
        world.xz += vec2(0.35, -0.18) * drift * smoothstep(0.0, 0.9, height);
        world.y += height * height * 0.085 + sin(time * 1.6 + position.x * 3.0) * 0.012;
    }
    vertexColor = color;
    worldPosition = world.xyz;
    worldNormal = transformedNormal;
    lightSpacePositions[0] = lightSpaceMatrices[0] * world;
    lightSpacePositions[1] = lightSpaceMatrices[1] * world;
    uv = vec2(textureCoordinate.x, 1.0 - textureCoordinate.y);
    gl_Position = projection * view * world;
}
