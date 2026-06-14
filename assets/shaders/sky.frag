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

float hashDirection(vec3 direction) {
    return fract(sin(dot(direction, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float rayleighPhase(float cosine) {
    return 0.0596831 * (1.0 + cosine * cosine);
}

float miePhase(float cosine) {
    const float anisotropy = 0.76;
    float denominator = 1.0 + anisotropy * anisotropy - 2.0 * anisotropy * cosine;
    return 0.119366 * (1.0 - anisotropy * anisotropy)
        / max(denominator * sqrt(denominator), 0.001);
}

float raySphereIntersection(vec3 rayDirection, vec3 center, float radius) {
    float projectedCenter = dot(rayDirection, center);
    float discriminant = projectedCenter * projectedCenter - dot(center, center) + radius * radius;
    if (discriminant < 0.0) {
        return -1.0;
    }
    return projectedCenter - sqrt(discriminant);
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

float cloudLayer(vec3 worldPosition) {
    const float cloudBottom = 105.0;
    const float cloudTop = 185.0;
    return smoothstep(cloudBottom, cloudBottom + 18.0, worldPosition.y)
        * (1.0 - smoothstep(cloudTop - 25.0, cloudTop, worldPosition.y));
}

float cloudDensity(vec3 worldPosition) {
    float layer = cloudLayer(worldPosition);
    if (layer <= 0.001) {
        return 0.0;
    }
    vec3 samplePosition = worldPosition * vec3(0.012, 0.018, 0.012)
        + vec3(time * 0.011, 0.0, time * 0.003);
    float shape = cloudNoise(samplePosition);
    float threshold = mix(0.68, 0.38, clamp(cloudiness, 0.0, 1.0));
    if (shape <= threshold) {
        return 0.0;
    }
    float erosion = valueNoise3(samplePosition * 3.2 - vec3(time * 0.019, 0.0, 0.0));
    return smoothstep(threshold, threshold + 0.18, shape - erosion * 0.12) * layer;
}

float cloudLightDensity(vec3 worldPosition) {
    float layer = cloudLayer(worldPosition);
    if (layer <= 0.001) {
        return 0.0;
    }
    vec3 samplePosition = worldPosition * vec3(0.012, 0.018, 0.012)
        + vec3(time * 0.011, 0.0, time * 0.003);
    float shape = valueNoise3(samplePosition) * 0.67
        + valueNoise3(samplePosition * 2.03 + vec3(7.1, 3.4, 9.2)) * 0.33;
    float threshold = mix(0.68, 0.38, clamp(cloudiness, 0.0, 1.0));
    return smoothstep(threshold, threshold + 0.18, shape) * layer;
}

vec4 volumetricClouds(vec3 rayOrigin, vec3 rayDirection, vec3 sunDir, float twilight) {
    if (rayDirection.y <= 0.0) {
        return vec4(0.0);
    }
    float horizonFade = smoothstep(0.008, 0.055, rayDirection.y);
    const float cloudBottom = 105.0;
    const float cloudTop = 185.0;
    float nearDistance = max((cloudBottom - rayOrigin.y) / rayDirection.y, 0.0);
    float farDistance = max((cloudTop - rayOrigin.y) / rayDirection.y, nearDistance);
    farDistance = min(farDistance, nearDistance + 650.0);
    float stepLength = (farDistance - nearDistance) / 18.0;
    float jitter = hash(screenPosition * 521.0);
    float distanceAlongRay = nearDistance + stepLength * jitter;
    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;
    float dayAmount = clamp(daylight, 0.0, 1.0);
    vec3 shadowColor = mix(vec3(0.055, 0.075, 0.12), vec3(0.30, 0.35, 0.40), dayAmount);
    vec3 litColor = mix(vec3(0.28, 0.34, 0.48), vec3(1.0, 0.72, 0.46), twilight);

    for (int stepIndex = 0; stepIndex < 18; ++stepIndex) {
        vec3 samplePoint = rayOrigin + rayDirection * distanceAlongRay;
        float density = cloudDensity(samplePoint);
        if (density > 0.01) {
            float lightDensity = cloudLightDensity(samplePoint + sunDir * 18.0);
            float directLight = exp(-lightDensity * 2.8);
            float forwardScatter = pow(max(dot(rayDirection, sunDir), 0.0), 8.0);
            vec3 sampleColor = mix(shadowColor, litColor, directLight * (0.55 + dayAmount * 0.45));
            sampleColor += vec3(1.0, 0.72, 0.42) * forwardScatter * dayAmount * 0.55;
            float opacity = 1.0 - exp(-density * stepLength * 0.026);
            accumulated += sampleColor * opacity * transmittance;
            transmittance *= 1.0 - opacity;
            if (transmittance < 0.025) {
                break;
            }
        }
        distanceAlongRay += stepLength;
    }
    return vec4(accumulated, 1.0 - transmittance) * horizonFade;
}

float lightningFlash(float currentTime, float amount) {
    float cycle = floor(currentTime / 7.0);
    float phase = fract(currentTime / 7.0);
    float flashActive = step(0.82, hash(vec2(cycle, 19.7))) * smoothstep(0.78, 0.9, amount);
    float first = exp(-pow((phase - 0.08) * 90.0, 2.0));
    float second = exp(-pow((phase - 0.14) * 120.0, 2.0));
    return flashActive * (first + second * 0.65);
}

vec3 starField(vec3 direction, float nightAmount, float cloudMask) {
    vec2 spherical = vec2(atan(direction.z, direction.x), asin(clamp(direction.y, -1.0, 1.0)));
    vec2 fineCell = floor(spherical * vec2(520.0, 340.0));
    vec2 coarseCell = floor(spherical * vec2(230.0, 150.0));
    float fineSeed = hash(fineCell);
    float coarseSeed = hash(coarseCell + 41.7);
    float fineStars = smoothstep(0.9965, 0.9998, fineSeed);
    float brightStars = smoothstep(0.995, 0.9998, coarseSeed) * 1.6;
    float twinkle = 0.72 + 0.28 * sin(time * (0.8 + coarseSeed * 2.4) + fineSeed * 35.0);
    vec3 starColor = mix(vec3(0.58, 0.72, 1.0), vec3(1.0, 0.78, 0.52), coarseSeed);

    vec3 galacticNormal = normalize(vec3(0.16, 0.93, 0.33));
    float galacticBand = exp(-pow(dot(direction, galacticNormal) * 7.0, 2.0));
    float galacticDust = hashDirection(floor(direction * 180.0));
    vec3 milkyWay = mix(vec3(0.035, 0.055, 0.11), vec3(0.13, 0.10, 0.16), galacticDust)
        * galacticBand * (0.35 + galacticDust * 0.65);
    float visibility = nightAmount * (1.0 - cloudMask) * smoothstep(-0.04, 0.16, direction.y);
    return (starColor * (fineStars + brightStars) * twinkle + milkyWay) * visibility;
}

void main() {
    vec4 viewDirection = inverseProjection * vec4(screenPosition, 1.0, 1.0);
    vec3 direction = normalize((inverseView * vec4(viewDirection.xyz, 0.0)).xyz);
    vec3 sunDir = normalize(sunDirection);
    vec3 moonDir = normalize(moonDirection);
    float dayAmount = clamp(daylight, 0.0, 1.0);
    float nightAmount = clamp(nightFactor, 0.0, 1.0);
    float cloudAmount = clamp(cloudiness, 0.0, 1.0);
    float elevation = clamp(direction.y, 0.0, 1.0);
    float horizonHaze = exp(-abs(direction.y) * 7.0);
    float sunAlignment = max(dot(direction, sunDir), 0.0);
    float twilight = (1.0 - dayAmount) * smoothstep(-0.2, 0.12, sunDirection.y);
    vec3 atmosphericHorizon = mix(horizonColor, vec3(1.0, 0.24, 0.055), twilight * 0.72);
    vec3 color = mix(atmosphericHorizon, zenithColor, pow(elevation, 0.48));
    float viewSunCosine = dot(direction, sunDir);
    float opticalDepth = 1.0 / max(direction.y + 0.12, 0.12);
    vec3 rayleighColor = vec3(0.18, 0.42, 0.95) * rayleighPhase(viewSunCosine);
    vec3 mieColor = vec3(1.0, 0.62, 0.28) * miePhase(viewSunCosine);
    color += (rayleighColor * 0.85 + mieColor * 0.11) * dayAmount / (1.0 + opticalDepth * 0.12);
    color += vec3(0.12, 0.22, 0.42) * pow(elevation, 2.4) * dayAmount * 0.3;
    color = mix(color, atmosphericHorizon * 1.18, horizonHaze * 0.42);

    const float celestialDistance = 1000.0;
    float sunHit = raySphereIntersection(direction, sunDir * celestialDistance, 18.0);
    float sun = step(0.0, sunHit);
    float glow = pow(sunAlignment, 14.0);
    float wideGlow = pow(sunAlignment, 3.0) * horizonHaze;
    color += vec3(1.0, 0.72, 0.38) * (sun * 6.5 + glow * 0.38 + wideGlow * 0.12) * dayAmount;
    float moonAlignment = max(dot(direction, moonDir), 0.0);
    vec3 moonCenter = moonDir * celestialDistance;
    float moonHit = raySphereIntersection(direction, moonCenter, 20.0);
    float moon = step(0.0, moonHit);
    if (moonHit >= 0.0) {
        vec3 moonNormal = normalize(direction * moonHit - moonCenter);
        float craters = hashDirection(floor(moonNormal * 34.0));
        float craterShade = mix(0.58, 1.08, smoothstep(0.18, 0.82, craters));
        moon *= (0.24 + 0.76 * max(dot(moonNormal, sunDir), 0.0)) * craterShade;
    }
    float moonGlow = pow(moonAlignment, 45.0);
    color += moonColor * (moon * 6.0 + moonGlow * 0.28) * nightAmount;

    vec4 clouds = volumetricClouds(cameraPosition, direction, sunDir, twilight);
    float cloudMask = clouds.a;
    color = color * (1.0 - clouds.a) + clouds.rgb;
    float silverLining = pow(sunAlignment, 12.0) * smoothstep(0.02, 0.35, clouds.a);
    color += vec3(1.0, 0.78, 0.52) * silverLining * dayAmount * (1.0 - clouds.a) * 0.5;

    color += starField(direction, nightAmount, cloudMask);
    float lightning = lightningFlash(time, cloudAmount);
    float lightningColumn = exp(-abs(direction.x + 0.22) * 35.0) * (1.0 - smoothstep(0.02, 0.72, direction.y));
    color += vec3(0.58, 0.68, 1.0) * lightning * (0.55 + lightningColumn * 2.8) * cloudMask;
    fragmentColor = vec4(color, 1.0);
}
