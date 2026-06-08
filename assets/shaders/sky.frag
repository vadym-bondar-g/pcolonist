#version 330 core

in vec2 screenPosition;
out vec4 fragmentColor;

uniform mat4 inverseProjection;
uniform mat4 inverseView;
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform vec3 sunDirection;
uniform vec3 moonDirection;
uniform vec3 moonColor;
uniform float cloudiness;
uniform float daylight;
uniform float nightFactor;
uniform float time;

float hash(vec2 position) {
    return fract(sin(dot(position, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 position) {
    vec2 cell = floor(position);
    vec2 local = fract(position);
    local = local * local * (3.0 - 2.0 * local);
    return mix(
        mix(hash(cell), hash(cell + vec2(1.0, 0.0)), local.x),
        mix(hash(cell + vec2(0.0, 1.0)), hash(cell + vec2(1.0)), local.x),
        local.y
    );
}

float fractalNoise(vec2 position) {
    float value = 0.0;
    float amplitude = 0.55;
    for (int octave = 0; octave < 5; ++octave) {
        value += noise(position) * amplitude;
        position = position * 2.03 + vec2(17.1, 9.2);
        amplitude *= 0.48;
    }
    return value;
}

void main() {
    vec4 viewDirection = inverseProjection * vec4(screenPosition, 1.0, 1.0);
    vec3 direction = normalize((inverseView * vec4(viewDirection.xyz, 0.0)).xyz);
    float height = clamp(direction.y * 0.65 + 0.35, 0.0, 1.0);
    vec3 color = mix(horizonColor, zenithColor, pow(height, 0.65));
    float horizonHaze = exp(-abs(direction.y) * 8.0);
    color = mix(color, horizonColor * 1.15, horizonHaze * 0.35);

    float sun = pow(max(dot(direction, normalize(sunDirection)), 0.0), 700.0);
    float glow = pow(max(dot(direction, normalize(sunDirection)), 0.0), 18.0);
    color += vec3(1.0, 0.72, 0.38) * (sun * 5.0 + glow * 0.35) * daylight;
    float moon = pow(max(dot(direction, normalize(moonDirection)), 0.0), 900.0);
    float moonGlow = pow(max(dot(direction, normalize(moonDirection)), 0.0), 45.0);
    color += moonColor * (moon * 6.0 + moonGlow * 0.28) * nightFactor;

    vec2 cloudsPosition = direction.xz / max(direction.y + 0.25, 0.2);
    float clouds = fractalNoise(cloudsPosition * 2.2 + vec2(time * 0.012, time * 0.002));
    clouds = smoothstep(0.72 - cloudiness * 0.32, 1.05 - cloudiness * 0.22, clouds) * smoothstep(0.0, 0.22, direction.y);
    float cloudLight = 0.5 + 0.5 * max(dot(direction, normalize(sunDirection)), 0.0);
    vec3 cloudColor = mix(vec3(0.16, 0.18, 0.24), vec3(0.76, 0.78, 0.8), daylight) * mix(0.72, 1.18, cloudLight);
    color = mix(color, cloudColor, clouds * mix(0.42, 0.92, cloudiness));

    float stars = step(0.9965, hash(floor(direction.xz * 480.0))) * nightFactor;
    float twinkle = 0.62 + 0.38 * sin(time * 2.0 + hash(floor(direction.xz * 480.0)) * 20.0);
    color += vec3(stars * twinkle) * smoothstep(0.0, 0.18, direction.y);
    fragmentColor = vec4(color, 1.0);
}
