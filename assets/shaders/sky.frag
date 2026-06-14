#version 330 core

in vec2 screenPosition;
out vec4 fragmentColor;

uniform mat4 inverseProjection;
uniform mat4 inverseView;
uniform vec3 cameraPosition;
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

float hash3(vec3 position) {
    position = fract(position * 0.1031);
    position += dot(position, position.yzx + 33.33);
    return fract((position.x + position.y) * position.z);
}

float valueNoise3(vec3 position) {
    vec3 cell = floor(position);
    vec3 local = fract(position);
    local = local * local * (3.0 - 2.0 * local);
    return mix(
        mix(
            mix(hash3(cell), hash3(cell + vec3(1.0, 0.0, 0.0)), local.x),
            mix(hash3(cell + vec3(0.0, 1.0, 0.0)), hash3(cell + vec3(1.0, 1.0, 0.0)), local.x),
            local.y
        ),
        mix(
            mix(hash3(cell + vec3(0.0, 0.0, 1.0)), hash3(cell + vec3(1.0, 0.0, 1.0)), local.x),
            mix(hash3(cell + vec3(0.0, 1.0, 1.0)), hash3(cell + vec3(1.0)), local.x),
            local.y
        ),
        local.z
    );
}

float cloudNoise(vec3 position) {
    float value = valueNoise3(position) * 0.58;
    value += valueNoise3(position * 2.03 + vec3(7.1, 3.4, 9.2)) * 0.29;
    value += valueNoise3(position * 4.11 + vec3(2.8, 8.3, 1.7)) * 0.13;
    return value;
}

float cloudDensity(vec3 worldPosition) {
    const float cloudBottom = 105.0;
    const float cloudTop = 185.0;
    float layer = smoothstep(cloudBottom, cloudBottom + 18.0, worldPosition.y)
        * (1.0 - smoothstep(cloudTop - 25.0, cloudTop, worldPosition.y));
    vec3 samplePosition = worldPosition * vec3(0.012, 0.018, 0.012)
        + vec3(time * 0.011, 0.0, time * 0.003);
    float shape = cloudNoise(samplePosition);
    float erosion = valueNoise3(samplePosition * 3.2 - vec3(time * 0.019, 0.0, 0.0));
    float threshold = mix(0.68, 0.38, cloudiness);
    return smoothstep(threshold, threshold + 0.18, shape - erosion * 0.12) * layer;
}

vec4 volumetricClouds(vec3 rayOrigin, vec3 rayDirection, vec3 sunDir, float twilight) {
    if (rayDirection.y <= 0.012) {
        return vec4(0.0);
    }
    const float cloudBottom = 105.0;
    const float cloudTop = 185.0;
    float nearDistance = max((cloudBottom - rayOrigin.y) / rayDirection.y, 0.0);
    float farDistance = max((cloudTop - rayOrigin.y) / rayDirection.y, nearDistance);
    farDistance = min(farDistance, nearDistance + 650.0);
    float stepLength = (farDistance - nearDistance) / 18.0;
    float jitter = hash(screenPosition + fract(time) * 0.01);
    float distanceAlongRay = nearDistance + stepLength * jitter;
    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;
    vec3 shadowColor = mix(vec3(0.055, 0.075, 0.12), vec3(0.30, 0.35, 0.40), daylight);
    vec3 litColor = mix(vec3(0.28, 0.34, 0.48), vec3(1.0, 0.72, 0.46), twilight);

    for (int stepIndex = 0; stepIndex < 18; ++stepIndex) {
        vec3 samplePoint = rayOrigin + rayDirection * distanceAlongRay;
        float density = cloudDensity(samplePoint);
        if (density > 0.01) {
            float lightDensity = cloudDensity(samplePoint + sunDir * 18.0);
            float directLight = exp(-lightDensity * 2.8);
            float forwardScatter = pow(max(dot(rayDirection, sunDir), 0.0), 8.0);
            vec3 sampleColor = mix(shadowColor, litColor, directLight * (0.55 + daylight * 0.45));
            sampleColor += vec3(1.0, 0.72, 0.42) * forwardScatter * daylight * 0.55;
            float opacity = 1.0 - exp(-density * stepLength * 0.026);
            accumulated += sampleColor * opacity * transmittance;
            transmittance *= 1.0 - opacity;
            if (transmittance < 0.025) {
                break;
            }
        }
        distanceAlongRay += stepLength;
    }
    return vec4(accumulated, 1.0 - transmittance);
}

float lightningFlash(float currentTime, float amount) {
    float cycle = floor(currentTime / 7.0);
    float phase = fract(currentTime / 7.0);
    float flashActive = step(0.82, hash(vec2(cycle, 19.7))) * smoothstep(0.78, 0.9, amount);
    float first = exp(-pow((phase - 0.08) * 90.0, 2.0));
    float second = exp(-pow((phase - 0.14) * 120.0, 2.0));
    return flashActive * (first + second * 0.65);
}

void main() {
    vec4 viewDirection = inverseProjection * vec4(screenPosition, 1.0, 1.0);
    vec3 direction = normalize((inverseView * vec4(viewDirection.xyz, 0.0)).xyz);
    vec3 sunDir = normalize(sunDirection);
    float elevation = clamp(direction.y, 0.0, 1.0);
    float horizonHaze = exp(-abs(direction.y) * 7.0);
    float sunAlignment = max(dot(direction, sunDir), 0.0);
    float twilight = (1.0 - daylight) * smoothstep(-0.2, 0.12, sunDirection.y);
    vec3 atmosphericHorizon = mix(horizonColor, vec3(1.0, 0.24, 0.055), twilight * 0.72);
    vec3 color = mix(atmosphericHorizon, zenithColor, pow(elevation, 0.48));
    color += vec3(0.12, 0.22, 0.42) * pow(elevation, 2.4) * daylight * 0.3;
    color = mix(color, atmosphericHorizon * 1.18, horizonHaze * 0.42);

    float sun = pow(sunAlignment, 1100.0);
    float glow = pow(sunAlignment, 14.0);
    float wideGlow = pow(sunAlignment, 3.0) * horizonHaze;
    color += vec3(1.0, 0.72, 0.38) * (sun * 6.5 + glow * 0.38 + wideGlow * 0.12) * daylight;
    float moon = pow(max(dot(direction, normalize(moonDirection)), 0.0), 900.0);
    float moonGlow = pow(max(dot(direction, normalize(moonDirection)), 0.0), 45.0);
    color += moonColor * (moon * 6.0 + moonGlow * 0.28) * nightFactor;

    vec4 clouds = volumetricClouds(cameraPosition, direction, sunDir, twilight);
    float cloudMask = clouds.a;
    color = color * (1.0 - clouds.a) + clouds.rgb;
    float silverLining = pow(sunAlignment, 12.0) * smoothstep(0.02, 0.35, clouds.a);
    color += vec3(1.0, 0.78, 0.52) * silverLining * daylight * (1.0 - clouds.a) * 0.5;

    vec2 starCell = floor(vec2(atan(direction.z, direction.x), asin(direction.y)) * vec2(360.0, 240.0));
    float starSeed = hash(starCell);
    float stars = step(0.9945, starSeed) * nightFactor * (1.0 - cloudMask);
    float twinkle = 0.58 + 0.42 * sin(time * (1.2 + starSeed * 2.0) + starSeed * 30.0);
    color += mix(vec3(0.65, 0.78, 1.0), vec3(1.0, 0.82, 0.58), starSeed) * stars * twinkle
        * smoothstep(-0.02, 0.16, direction.y);
    float lightning = lightningFlash(time, cloudiness);
    float lightningColumn = exp(-abs(direction.x + 0.22) * 35.0) * (1.0 - smoothstep(0.02, 0.72, direction.y));
    color += vec3(0.58, 0.68, 1.0) * lightning * (0.55 + lightningColumn * 2.8) * cloudMask;
    fragmentColor = vec4(color, 1.0);
}
