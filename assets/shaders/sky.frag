#version 330 core

in vec2 screenPosition;
out vec4 fragmentColor;

uniform mat4 inverseProjection;
uniform mat4 inverseView;
uniform vec3 cameraPosition;
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform vec3 fogColor;
uniform vec3 sunDirection;
uniform vec3 moonDirection;
uniform vec3 moonColor;
uniform float cloudiness;
uniform float stormStrength;
uniform float hazeAmount;
uniform float starVisibility;
uniform float moonPhase;
uniform vec2 cloudWind;
uniform int skyQuality;
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

float segmentDistance(vec2 point, vec2 start, vec2 end) {
    vec2 segment = end - start;
    float progress = clamp(dot(point - start, segment) / max(dot(segment, segment), 0.0001), 0.0, 1.0);
    return length(point - (start + segment * progress));
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

float cloudHeight01(vec3 worldPosition) {
    const float cloudBottom = 105.0;
    const float cloudTop = 205.0;
    return clamp((worldPosition.y - cloudBottom) / (cloudTop - cloudBottom), 0.0, 1.0);
}

float cloudLayer(vec3 worldPosition) {
    float height = cloudHeight01(worldPosition);
    float base = smoothstep(0.0, 0.18, height);
    float top = 1.0 - smoothstep(0.72, 1.0, height);
    return base * top;
}

float cloudCore(vec3 samplePosition, float height) {
    vec2 cell = floor(samplePosition.xz * 0.52);
    vec2 local = fract(samplePosition.xz * 0.52) - 0.5;
    float cellSeed = hash(cell);
    float radius = mix(0.26, 0.56, cellSeed);
    float dome = 1.0 - smoothstep(radius * 0.45, radius, length(local));
    float verticalBulge = smoothstep(0.05, 0.44, height) * (1.0 - smoothstep(0.52, 0.98, height));
    return dome * verticalBulge * smoothstep(0.18, 0.88, cellSeed);
}

float cloudDensity(vec3 worldPosition) {
    float layer = cloudLayer(worldPosition);
    if (layer <= 0.001) {
        return 0.0;
    }
    float height = cloudHeight01(worldPosition);
    vec3 samplePosition = worldPosition * vec3(0.012, 0.018, 0.012)
        + vec3(time * cloudWind.x, 0.0, time * cloudWind.y);
    float broadShape = cloudNoise(samplePosition * vec3(0.72, 0.5, 0.72));
    float billow = cloudNoise(samplePosition * vec3(1.35, 0.8, 1.35) + vec3(9.1, 0.7, 3.4));
    float puffs = cloudCore(samplePosition, height);
    float shape = broadShape * 0.62 + billow * 0.26 + puffs * 0.36;
    float threshold = mix(0.64, 0.34, clamp(cloudiness, 0.0, 1.0));
    if (shape <= threshold - 0.035) {
        return 0.0;
    }
    float erosion = valueNoise3(samplePosition * 3.2 - vec3(time * (0.012 + stormStrength * 0.018), 0.0, 0.0));
    float stormLift = smoothstep(0.72, 0.98, stormStrength) * smoothstep(0.48, 0.76, shape);
    float cauliflower = smoothstep(0.34, 0.82, billow) * smoothstep(0.2, 0.62, height);
    float density = smoothstep(threshold, threshold + 0.2, shape - erosion * 0.11);
    density *= mix(0.72, 1.32, cauliflower);
    density *= layer * (1.0 + stormLift * 0.5);
    density *= mix(0.72, 1.18, 1.0 - height);
    return density;
}

float cloudLightDensity(vec3 worldPosition) {
    float layer = cloudLayer(worldPosition);
    if (layer <= 0.001) {
        return 0.0;
    }
    vec3 samplePosition = worldPosition * vec3(0.012, 0.018, 0.012)
        + vec3(time * cloudWind.x, 0.0, time * cloudWind.y);
    float shape = valueNoise3(samplePosition) * 0.67
        + valueNoise3(samplePosition * 2.03 + vec3(7.1, 3.4, 9.2)) * 0.33;
    float threshold = mix(0.68, 0.38, clamp(cloudiness, 0.0, 1.0));
    return smoothstep(threshold, threshold + 0.18, shape) * layer;
}

float cloudSelfShadow(vec3 worldPosition, vec3 sunDir) {
    if (skyQuality <= 1) {
        return 0.0;
    }
    float shadow = 0.0;
    shadow += cloudDensity(worldPosition + sunDir * 20.0) * 0.45;
    if (skyQuality >= 3) {
        shadow += cloudDensity(worldPosition + sunDir * 52.0) * 0.30;
    }
    return clamp(shadow, 0.0, 1.0);
}

float cirrusClouds(vec3 direction, float sunAmount) {
    if (direction.y <= 0.05) {
        return 0.0;
    }
    vec2 wind = cloudWind * time * vec2(0.42, 0.55);
    vec2 uv = direction.xz / max(direction.y + 0.24, 0.24) * 0.46 + wind;
    float streaks = valueNoise3(vec3(uv * vec2(8.0, 1.45), 2.3));
    streaks += valueNoise3(vec3(uv * vec2(18.0, 2.1) + vec2(4.7, 1.9), 6.1)) * 0.42;
    float wisps = smoothstep(0.72, 1.08, streaks);
    float coverage = (1.0 - cloudiness) * 0.55 + smoothstep(0.18, 0.72, cloudiness) * 0.18;
    return wisps * coverage * smoothstep(0.12, 0.72, direction.y) * (0.45 + sunAmount * 0.55);
}

float stormShelfCloud(vec3 direction, float cloudAmount) {
    if (direction.y <= 0.0) {
        return 0.0;
    }
    float horizonBand = smoothstep(0.015, 0.11, direction.y) * (1.0 - smoothstep(0.22, 0.48, direction.y));
    vec2 uv = direction.xz / max(direction.y + 0.08, 0.08) * 0.18 + cloudWind * time * vec2(0.55, -0.38);
    float roll = cloudNoise(vec3(uv.x * 2.6, 1.5, uv.y * 1.3));
    return horizonBand * smoothstep(0.64, 0.86, roll) * smoothstep(0.35, 0.95, stormStrength);
}

vec4 volumetricClouds(vec3 rayOrigin, vec3 rayDirection, vec3 sunDir, float twilight) {
    if (rayDirection.y <= 0.0 || skyQuality <= 0 || cloudiness < 0.08) {
        return vec4(0.0);
    }
    float horizonFade = smoothstep(0.008, 0.055, rayDirection.y);
    const float cloudBottom = 105.0;
    const float cloudTop = 205.0;
    float nearDistance = max((cloudBottom - rayOrigin.y) / rayDirection.y, 0.0);
    float farDistance = max((cloudTop - rayOrigin.y) / rayDirection.y, nearDistance);
    farDistance = min(farDistance, nearDistance + 650.0);
    const int maxCloudSteps = 16;
    int activeSteps = skyQuality <= 1 ? 8 : (skyQuality == 2 ? 12 : 16);
    activeSteps = min(activeSteps, cloudiness < 0.25 ? 6 : activeSteps);
    float stepLength = (farDistance - nearDistance) / float(activeSteps);
    float jitter = hash(screenPosition * 521.0);
    float distanceAlongRay = nearDistance + stepLength * jitter;
    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;
    float dayAmount = clamp(daylight, 0.0, 1.0);
    vec3 shadowColor = mix(vec3(0.055, 0.075, 0.12), vec3(0.30, 0.35, 0.40), dayAmount);
    shadowColor = mix(shadowColor, vec3(0.09, 0.10, 0.115), smoothstep(0.18, 1.0, stormStrength));
    vec3 litColor = mix(vec3(0.28, 0.34, 0.48), vec3(1.0, 0.72, 0.46), twilight);
    litColor = mix(litColor, vec3(0.78, 0.80, 0.82), smoothstep(0.35, 0.78, cloudiness) * dayAmount * (1.0 - twilight));

    for (int stepIndex = 0; stepIndex < maxCloudSteps; ++stepIndex) {
        if (stepIndex >= activeSteps) {
            break;
        }
        vec3 samplePoint = rayOrigin + rayDirection * distanceAlongRay;
        float density = cloudDensity(samplePoint);
        if (density > 0.01) {
            float height = cloudHeight01(samplePoint);
            float lightDensity = cloudLightDensity(samplePoint + sunDir * 18.0);
            float selfShadow = cloudSelfShadow(samplePoint, sunDir);
            float directLight = exp(-(lightDensity * 2.4 + selfShadow * 3.1));
            float forwardScatter = pow(max(dot(rayDirection, sunDir), 0.0), 8.0);
            float bottomShade = mix(0.58, 1.12, smoothstep(0.08, 0.78, height));
            float edgeLight = pow(1.0 - clamp(density, 0.0, 1.0), 2.0) * directLight;
            vec3 sampleColor = mix(shadowColor, litColor, directLight * (0.48 + dayAmount * 0.52));
            sampleColor *= bottomShade;
            sampleColor += vec3(1.0, 0.72, 0.42) * forwardScatter * dayAmount * (0.42 + twilight * 0.34);
            sampleColor += vec3(1.0, 0.82, 0.56) * edgeLight * dayAmount * (0.18 + twilight * 0.18);
            float opacity = 1.0 - exp(-density * stepLength * mix(0.026, 0.048, stormStrength));
            accumulated += sampleColor * opacity * transmittance;
            transmittance *= 1.0 - opacity;
            if (transmittance < 0.055) {
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

vec3 constellationLines(vec2 skyUv) {
    float hunter = 0.0;
    hunter += 1.0 - smoothstep(0.002, 0.012, segmentDistance(skyUv, vec2(0.58, 0.59), vec2(0.62, 0.64)));
    hunter += 1.0 - smoothstep(0.002, 0.012, segmentDistance(skyUv, vec2(0.62, 0.64), vec2(0.66, 0.58)));
    hunter += 1.0 - smoothstep(0.002, 0.010, segmentDistance(skyUv, vec2(0.60, 0.53), vec2(0.64, 0.53)));
    hunter += 1.0 - smoothstep(0.002, 0.012, segmentDistance(skyUv, vec2(0.62, 0.64), vec2(0.62, 0.53)));

    float sailor = 0.0;
    sailor += 1.0 - smoothstep(0.002, 0.011, segmentDistance(skyUv, vec2(0.26, 0.62), vec2(0.31, 0.68)));
    sailor += 1.0 - smoothstep(0.002, 0.011, segmentDistance(skyUv, vec2(0.31, 0.68), vec2(0.38, 0.66)));
    sailor += 1.0 - smoothstep(0.002, 0.011, segmentDistance(skyUv, vec2(0.38, 0.66), vec2(0.42, 0.59)));

    return vec3(0.42, 0.58, 1.0) * clamp(hunter + sailor, 0.0, 1.0);
}

vec3 starField(vec3 direction, float nightAmount, float cloudMask, float moonGlow) {
    vec2 spherical = vec2(atan(direction.z, direction.x), asin(clamp(direction.y, -1.0, 1.0)));
    vec2 skyUv = vec2(spherical.x / 6.2831853 + 0.5, spherical.y / 3.14159265 + 0.5);
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
    vec3 constellations = constellationLines(skyUv) * smoothstep(0.18, 0.72, direction.y);
    float visibility = starVisibility * nightAmount * (1.0 - cloudMask) * smoothstep(-0.04, 0.18, direction.y);
    visibility *= 1.0 - clamp(moonGlow * 1.35 + hazeAmount * 0.38, 0.0, 0.78);
    return (starColor * (fineStars + brightStars) * twinkle + milkyWay + constellations * 0.32) * visibility;
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
    float sunElevation = sunDirection.y;
    float twilight = (1.0 - dayAmount) * smoothstep(-0.24, 0.14, sunElevation);
    float dawnDusk = exp(-pow((sunElevation + 0.02) * 5.4, 2.0));
    vec3 warmHorizon = mix(vec3(1.0, 0.38, 0.08), vec3(0.78, 0.17, 0.22), smoothstep(0.42, 0.92, cloudAmount));
    vec3 violetBelt = vec3(0.22, 0.18, 0.42) * dawnDusk * (1.0 - dayAmount * 0.45);
    vec3 atmosphericHorizon = mix(horizonColor, warmHorizon, twilight * (0.62 + horizonHaze * 0.18));
    atmosphericHorizon = mix(atmosphericHorizon, fogColor, horizonHaze * (0.18 + hazeAmount * 0.38));
    vec3 color = mix(atmosphericHorizon + violetBelt * smoothstep(0.08, 0.38, elevation), zenithColor, pow(elevation, 0.48));
    float viewSunCosine = dot(direction, sunDir);
    float opticalDepth = 1.0 / max(direction.y + 0.12, 0.12);
    vec3 rayleighColor = vec3(0.18, 0.42, 0.95) * rayleighPhase(viewSunCosine);
    vec3 mieColor = vec3(1.0, 0.62, 0.28) * miePhase(viewSunCosine);
    color += (rayleighColor * 0.85 + mieColor * 0.11) * dayAmount / (1.0 + opticalDepth * 0.12);
    color += vec3(0.12, 0.22, 0.42) * pow(elevation, 2.4) * dayAmount * 0.3;
    color += warmHorizon * dawnDusk * horizonHaze * (0.38 + (1.0 - cloudAmount) * 0.22);
    color = mix(color, atmosphericHorizon * 1.18, horizonHaze * (0.38 + hazeAmount * 0.28));

    const float celestialDistance = 1000.0;
    float sunHit = raySphereIntersection(direction, sunDir * celestialDistance, 18.0);
    float sun = step(0.0, sunHit);
    float glow = pow(sunAlignment, 14.0);
    float wideGlow = pow(sunAlignment, 3.0) * horizonHaze;
    float sunsetBloom = pow(sunAlignment, 5.0) * dawnDusk;
    color += vec3(1.0, 0.72, 0.38) * (sun * 6.5 + glow * 0.38 + wideGlow * 0.12) * dayAmount;
    color += warmHorizon * sunsetBloom * (0.42 + horizonHaze * 0.9) * (1.0 - cloudAmount * 0.35);
    float moonAlignment = max(dot(direction, moonDir), 0.0);
    vec3 moonCenter = moonDir * celestialDistance;
    float moonHit = raySphereIntersection(direction, moonCenter, 20.0);
    float moon = step(0.0, moonHit);
    if (moonHit >= 0.0) {
        vec3 moonNormal = normalize(direction * moonHit - moonCenter);
        float craters = hashDirection(floor(moonNormal * 34.0));
        float craterShade = mix(0.58, 1.08, smoothstep(0.18, 0.82, craters));
        float terminator = smoothstep(-0.08, 0.08, moonNormal.x + moonPhase * 0.72);
        float crescent = mix(terminator, 1.0 - terminator, step(moonPhase, 0.0));
        float phaseGlow = mix(0.28, 1.0, abs(moonPhase));
        moon *= (0.18 + 0.82 * crescent * phaseGlow) * craterShade;
    }
    float moonGlow = pow(moonAlignment, 45.0);
    color += moonColor * (moon * 6.0 + moonGlow * 0.28) * nightAmount;

    vec4 clouds = volumetricClouds(cameraPosition, direction, sunDir, twilight);
    float cloudMask = clouds.a;
    color = color * (1.0 - clouds.a) + clouds.rgb;
    float silverLining = pow(sunAlignment, 12.0) * smoothstep(0.02, 0.35, clouds.a);
    color += vec3(1.0, 0.78, 0.52) * silverLining * dayAmount * (1.0 - clouds.a) * (0.42 + twilight * 0.42);

    float cirrus = skyQuality > 0 ? cirrusClouds(direction, dayAmount) : 0.0;
    vec3 cirrusColor = mix(vec3(0.76, 0.80, 0.9), vec3(1.0, 0.52, 0.30), twilight);
    color = mix(color, color + cirrusColor * (0.24 + twilight * 0.38), cirrus);

    float shelf = skyQuality > 0 ? stormShelfCloud(direction, cloudAmount) : 0.0;
    vec3 shelfColor = mix(vec3(0.10, 0.12, 0.14), vec3(0.25, 0.27, 0.30), dayAmount);
    color = mix(color, shelfColor, shelf * 0.74);
    cloudMask = max(cloudMask, max(cirrus * 0.35, shelf));

    color += starField(direction, nightAmount, cloudMask, moonGlow);
    float lightning = lightningFlash(time, stormStrength);
    float lightningColumn = exp(-abs(direction.x + 0.22) * 35.0) * (1.0 - smoothstep(0.02, 0.72, direction.y));
    color += vec3(0.58, 0.68, 1.0) * lightning * (0.55 + lightningColumn * 2.8) * cloudMask;
    fragmentColor = vec4(color, 1.0);
}
