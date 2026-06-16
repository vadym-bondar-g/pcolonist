#version 330 core

in vec2 textureCoordinate;
out vec4 fragmentColor;

uniform sampler2D hdrScene;
uniform sampler2D sceneDepth;
uniform vec3 resolution;
uniform float exposure;
uniform float daylight;
uniform float time;
uniform float cloudiness;
uniform int bloomEnabled;
uniform int underwater;

const float cameraNear = 0.1;
const float cameraFar = 760.0;

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float linearDepth(float depth) {
    float ndc = depth * 2.0 - 1.0;
    return (2.0 * cameraNear * cameraFar)
        / (cameraFar + cameraNear - ndc * (cameraFar - cameraNear));
}

vec3 bright(vec3 color) {
    float brightness = luminance(color);
    float contribution = smoothstep(0.95, 2.8, brightness);
    return color * contribution * contribution;
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

float stableHash(vec2 position) {
    return fract(sin(dot(position, vec2(127.1, 311.7))) * 43758.5453);
}

float smoothNoise(vec2 position) {
    vec2 cell = floor(position);
    vec2 local = smoothstep(0.0, 1.0, fract(position));
    return mix(
        mix(noise(cell), noise(cell + vec2(1.0, 0.0)), local.x),
        mix(noise(cell + vec2(0.0, 1.0)), noise(cell + vec2(1.0)), local.x),
        local.y
    );
}

float lightningFlash() {
    float cycle = floor(time / 7.0);
    float phase = fract(time / 7.0);
    float flashActive = step(0.82, stableHash(vec2(cycle, 19.7)))
        * smoothstep(0.78, 0.9, cloudiness);
    return flashActive * (
        exp(-pow((phase - 0.08) * 90.0, 2.0))
        + exp(-pow((phase - 0.14) * 120.0, 2.0)) * 0.65
    );
}

float screenSpaceOcclusion(vec2 uv, vec2 texel) {
    float rawCenter = texture(sceneDepth, uv).r;
    if (rawCenter >= 0.9999) {
        return 1.0;
    }
    float center = linearDepth(rawCenter);
    float occlusion = 0.0;
    const vec2 directions[12] = vec2[12](
        vec2(1.0, 0.0), vec2(0.866, 0.5), vec2(0.5, 0.866),
        vec2(0.0, 1.0), vec2(-0.5, 0.866), vec2(-0.866, 0.5),
        vec2(-1.0, 0.0), vec2(-0.866, -0.5), vec2(-0.5, -0.866),
        vec2(0.0, -1.0), vec2(0.5, -0.866), vec2(0.866, -0.5)
    );
    float radius = mix(2.0, 7.0, smoothstep(5.0, 100.0, center));
    for (int index = 0; index < 12; ++index) {
        float sampleDepth = linearDepth(texture(sceneDepth, uv + directions[index] * texel * radius).r);
        float depthDifference = center - sampleDepth;
        float rangeWeight = 1.0 - smoothstep(0.2, 4.5, abs(depthDifference));
        occlusion += smoothstep(0.08, 1.4, depthDifference) * rangeWeight;
    }
    float distanceFade = 1.0 - smoothstep(55.0, 180.0, center);
    return 1.0 - occlusion / 12.0 * 0.5 * distanceFade;
}

vec3 multiScaleBloom(vec2 uv, vec2 texel) {
    vec3 bloom = bright(texture(hdrScene, uv).rgb) * 0.14;
    const vec2 directions[4] = vec2[4](
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.7071, 0.7071),
        vec2(0.7071, -0.7071)
    );
    for (int directionIndex = 0; directionIndex < 4; ++directionIndex) {
        vec2 direction = directions[directionIndex] * texel;
        bloom += bright(texture(hdrScene, uv + direction * 2.0).rgb) * 0.075;
        bloom += bright(texture(hdrScene, uv - direction * 2.0).rgb) * 0.075;
        bloom += bright(texture(hdrScene, uv + direction * 7.0).rgb) * 0.04;
        bloom += bright(texture(hdrScene, uv - direction * 7.0).rgb) * 0.04;
    }
    return bloom;
}

void main() {
    vec2 uv = textureCoordinate;
    if (underwater != 0) {
        uv.x += sin(uv.y * 38.0 + time * 1.7) * 0.0025;
        uv.y += cos(uv.x * 31.0 - time * 1.3) * 0.0018;
    }
    vec2 texel = 1.0 / resolution.xy;
    vec3 color = fxaa(uv, texel);
    vec3 localBlur = (
        texture(hdrScene, uv + vec2(texel.x, 0.0)).rgb
        + texture(hdrScene, uv - vec2(texel.x, 0.0)).rgb
        + texture(hdrScene, uv + vec2(0.0, texel.y)).rgb
        + texture(hdrScene, uv - vec2(0.0, texel.y)).rgb
    ) * 0.25;
    float rawDepth = texture(sceneDepth, uv).r;
    float viewDepth = rawDepth < 0.9999 ? linearDepth(rawDepth) : cameraFar;
    float geometryMask = 1.0 - smoothstep(0.9992, 0.99995, rawDepth);
    float detailFade = 1.0 - smoothstep(55.0, 260.0, viewDepth);
    color += (color - localBlur) * 0.14 * detailFade * geometryMask;
    color *= screenSpaceOcclusion(uv, texel);
    if (bloomEnabled != 0) {
        color += multiScaleBloom(uv, texel) * 0.46;
    }

    float aerialPerspective = rawDepth < 0.9999
        ? smoothstep(120.0, 620.0, viewDepth) * (0.045 + cloudiness * 0.13)
        : 0.0;
    vec3 aerialColor = mix(vec3(0.12, 0.18, 0.28), vec3(0.42, 0.58, 0.72), daylight);
    color = mix(color, aerialColor, aerialPerspective);
    color = acesToneMap(color * exposure);
    vec3 shadows = vec3(0.94, 0.97, 1.06);
    vec3 highlights = mix(vec3(1.02, 0.96, 0.9), vec3(1.03, 1.0, 0.94), daylight);
    color *= mix(shadows, highlights, smoothstep(0.15, 0.85, luminance(color)));
    color = mix(vec3(luminance(color)), color, mix(1.04, 1.11, daylight));
    color = (color - 0.5) * 1.035 + 0.5;
    if (underwater != 0) {
        float depthHaze = smoothstep(0.0, 1.0, uv.y);
        color = mix(color, vec3(0.015, 0.16, 0.2), 0.46 + depthHaze * 0.12);
        color *= vec3(0.66, 0.92, 1.02);
        float causticsA = abs(sin((uv.x + uv.y * 0.62) * 92.0 + time * 1.8));
        float causticsB = abs(sin((uv.x * 0.71 - uv.y) * 117.0 - time * 1.35));
        float caustics = pow(max(causticsA * causticsB, 0.0), 7.0);
        color += vec3(0.04, 0.18, 0.16) * caustics * daylight * (1.0 - depthHaze) * 0.45;
        float particles = step(0.992, smoothNoise(gl_FragCoord.xy * 0.09 + vec2(0.0, time * 0.7)));
        color += vec3(0.15, 0.28, 0.25) * particles * 0.18;
    }
    color += vec3(0.54, 0.62, 0.9) * lightningFlash() * 0.18;
    float vignette = smoothstep(0.95, 0.25, length(uv - 0.5));
    color *= mix(0.78, 1.0, vignette);
    color += (noise(gl_FragCoord.xy) - 0.5) / 255.0;
    color = pow(max(color, 0.0), vec3(1.0 / 2.2));
    fragmentColor = vec4(color, 1.0);
}
