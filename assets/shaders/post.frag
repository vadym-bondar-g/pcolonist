#version 330 core

in vec2 textureCoordinate;
out vec4 fragmentColor;

uniform sampler2D hdrScene;
uniform vec3 resolution;
uniform float exposure;
uniform float daylight;
uniform float time;
uniform int bloomEnabled;

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 bright(vec3 color) {
    return color * smoothstep(0.9, 2.2, luminance(color));
}

vec3 fxaa(vec2 uv, vec2 texel) {
    vec3 center = texture(hdrScene, uv).rgb;
    vec3 northWest = texture(hdrScene, uv + vec2(-1.0, 1.0) * texel).rgb;
    vec3 northEast = texture(hdrScene, uv + vec2(1.0, 1.0) * texel).rgb;
    vec3 southWest = texture(hdrScene, uv + vec2(-1.0, -1.0) * texel).rgb;
    vec3 southEast = texture(hdrScene, uv + vec2(1.0, -1.0) * texel).rgb;
    float centerLuma = luminance(center);
    float minimumLuma = min(centerLuma, min(min(luminance(northWest), luminance(northEast)), min(luminance(southWest), luminance(southEast))));
    float maximumLuma = max(centerLuma, max(max(luminance(northWest), luminance(northEast)), max(luminance(southWest), luminance(southEast))));
    if (maximumLuma - minimumLuma < max(0.035, maximumLuma * 0.1)) {
        return center;
    }
    vec2 direction = vec2(
        -((luminance(northWest) + luminance(northEast)) - (luminance(southWest) + luminance(southEast))),
        (luminance(northWest) + luminance(southWest)) - (luminance(northEast) + luminance(southEast))
    );
    direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + 0.001), vec2(-8.0), vec2(8.0)) * texel;
    return 0.5 * (
        texture(hdrScene, uv + direction * 0.333).rgb +
        texture(hdrScene, uv - direction * 0.333).rgb
    );
}

vec3 acesToneMap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float noise(vec2 position) {
    return fract(sin(dot(position, vec2(12.9898, 78.233)) + time) * 43758.5453);
}

void main() {
    vec2 uv = textureCoordinate;
    vec2 texel = 1.0 / resolution.xy;
    vec3 color = fxaa(uv, texel);
    if (bloomEnabled != 0) {
        vec3 bloom = vec3(0.0);
        float weight = 0.0;
        for (int x = -3; x <= 3; ++x) {
            for (int y = -3; y <= 3; ++y) {
                float sampleWeight = 1.0 / (1.0 + float(x * x + y * y));
                bloom += bright(texture(hdrScene, uv + vec2(x, y) * texel * 2.0).rgb) * sampleWeight;
                weight += sampleWeight;
            }
        }
        color += bloom / weight * 0.38;
    }

    color = acesToneMap(color * exposure);
    vec3 shadows = vec3(0.94, 0.97, 1.06);
    vec3 highlights = mix(vec3(1.02, 0.96, 0.9), vec3(1.03, 1.0, 0.94), daylight);
    color *= mix(shadows, highlights, smoothstep(0.15, 0.85, luminance(color)));
    color = mix(vec3(luminance(color)), color, 1.08);
    float vignette = smoothstep(0.95, 0.25, length(uv - 0.5));
    color *= mix(0.78, 1.0, vignette);
    color += (noise(gl_FragCoord.xy) - 0.5) / 255.0;
    color = pow(max(color, 0.0), vec3(1.0 / 2.2));
    fragmentColor = vec4(color, 1.0);
}
