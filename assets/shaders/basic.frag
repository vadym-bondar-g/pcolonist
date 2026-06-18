#version 330 core

in vec3 vertexColor;
in vec3 worldPosition;
in vec3 worldNormal;
in vec4 lightSpacePositions[2];
in vec2 uv;
out vec4 fragmentColor;

uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 moonDirection;
uniform vec3 moonColor;
uniform vec3 ambientColor;
uniform float fogDensity;
uniform float daylight;
uniform float nightFactor;
uniform float cloudiness;
uniform float water;
uniform float terrain;
uniform float lava;
uniform float fire;
uniform float smoke;
uniform float time;
uniform float roughness;
uniform float specularStrength;
uniform vec3 emissiveColor;
uniform sampler2D shadowMapNear;
uniform sampler2D shadowMapFar;
uniform sampler2D diffuseTexture;
uniform sampler2D terrainEarthTexture;
uniform sampler2D terrainSandTexture;
uniform sampler2D terrainBasaltTexture;
uniform int shadowsEnabled;
uniform int hasDiffuseTexture;
uniform int fireLightCount;
uniform vec3 fireLightPositions[4];
uniform vec3 fireLightColors[4];
uniform float fireLightIntensities[4];
uniform float fireLightFalloffs[4];

float hash(vec3 position) {
    return fract(sin(dot(position, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

float valueNoise(vec2 position) {
    vec2 cell = floor(position);
    vec2 blend = smoothstep(0.0, 1.0, fract(position));
    float a = hash(vec3(cell, 0.0));
    float b = hash(vec3(cell + vec2(1.0, 0.0), 0.0));
    float c = hash(vec3(cell + vec2(0.0, 1.0), 0.0));
    float d = hash(vec3(cell + vec2(1.0, 1.0), 0.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float terrainDetail(vec2 position) {
    return valueNoise(position * 0.16) * 0.5
        + valueNoise(position * 0.73) * 0.3
        + valueNoise(position * 3.1) * 0.2;
}

float cellular(vec2 position) {
    vec2 cell = floor(position);
    vec2 local = fract(position);
    float distanceToCell = 1.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(x, y);
            vec2 point = vec2(
                hash(vec3(cell + offset, 3.1)),
                hash(vec3(cell + offset, 7.7))
            );
            distanceToCell = min(distanceToCell, length(offset + point - local));
        }
    }
    return distanceToCell;
}

vec2 terrainGradient(vec2 position) {
    const float stepSize = 0.08;
    return vec2(
        terrainDetail(position + vec2(stepSize, 0.0)) - terrainDetail(position - vec2(stepSize, 0.0)),
        terrainDetail(position + vec2(0.0, stepSize)) - terrainDetail(position - vec2(0.0, stepSize))
    ) / (stepSize * 2.0);
}

vec3 triplanarSample(sampler2D material, vec3 position, vec3 normal, float scale) {
    vec3 weights = pow(abs(normal), vec3(5.0));
    weights /= max(weights.x + weights.y + weights.z, 0.0001);
    vec3 xProjection = texture(material, position.zy * scale).rgb;
    vec3 yProjection = texture(material, position.xz * scale).rgb;
    vec3 zProjection = texture(material, position.xy * scale).rgb;
    return xProjection * weights.x + yProjection * weights.y + zProjection * weights.z;
}

vec3 objectMaterialDetail(vec3 albedo, vec3 position, vec3 normal) {
    float green = smoothstep(0.03, 0.16, albedo.g - max(albedo.r, albedo.b));
    float brown = smoothstep(0.015, 0.14, albedo.r - albedo.b)
        * smoothstep(0.015, 0.22, albedo.g - albedo.b);
    float neutral = 1.0 - smoothstep(0.035, 0.18, max(albedo.r, max(albedo.g, albedo.b))
        - min(albedo.r, min(albedo.g, albedo.b)));
    float fibers = valueNoise(vec2(position.y * 4.6, atan(position.z, position.x) * 2.2));
    float leafClusters = terrainDetail(position.xz * 2.8 + position.y * 0.71);
    float pores = cellular(position.xz * 5.2 + normal.xz * 1.7);
    float cloth = valueNoise(position.xz * 12.0 + position.y * vec2(2.3, 3.7));
    float barkStripes = smoothstep(0.56, 0.92, valueNoise(vec2(atan(position.z, position.x) * 8.0, position.y * 2.8)));
    float leafVeins = smoothstep(0.72, 0.95, valueNoise(position.xz * 14.0 + position.y * vec2(0.7, 1.3)));
    float lichen = smoothstep(0.68, 0.88, terrainDetail(position.xz * 3.4 + normal.xz * 1.8)) * neutral;
    vec3 detailed = albedo;
    detailed *= mix(0.84, 1.13, fibers * brown);
    detailed = mix(detailed, detailed * vec3(0.62, 0.46, 0.30), barkStripes * brown * 0.34);
    detailed *= mix(0.82, 1.16, leafClusters * green);
    detailed = mix(detailed, detailed * vec3(0.74, 1.10, 0.62), leafVeins * green * 0.22);
    detailed *= mix(0.86, 1.08, smoothstep(0.12, 0.62, pores) * neutral);
    detailed = mix(detailed, detailed * vec3(0.70, 0.88, 0.66), lichen * 0.28);
    detailed *= mix(0.95, 1.05, cloth * (1.0 - max(green, max(brown, neutral))));
    return detailed;
}

float cascadeShadow(sampler2D shadowMap, vec4 lightSpacePosition, vec3 normal, vec3 lightDirection) {
    if (shadowsEnabled == 0) {
        return 0.0;
    }
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.z > 1.0) {
        return 0.0;
    }
    float bias = max(0.0015 * (1.0 - dot(normal, lightDirection)), 0.00035);
    vec2 texel = 1.0 / textureSize(shadowMap, 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(shadowMap, projected.xy + vec2(x, y) * texel).r;
            shadow += projected.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float shadowAmount(vec3 normal, vec3 lightDirection) {
    float distanceToCamera = length(cameraPosition - worldPosition);
    float nearShadow = cascadeShadow(shadowMapNear, lightSpacePositions[0], normal, lightDirection);
    if (distanceToCamera < 28.0) {
        return nearShadow;
    }
    float farShadow = cascadeShadow(shadowMapFar, lightSpacePositions[1], normal, lightDirection);
    return mix(nearShadow, farShadow, smoothstep(28.0, 42.0, distanceToCamera));
}

void main() {
    vec3 normal = normalize(worldNormal);
    float alpha = 1.0;
    if (fire > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        float width = mix(0.42, 0.045, smoothstep(0.0, 1.0, height));
        float wobble = (valueNoise(vec2(height * 4.2, time * 2.1)) - 0.5) * 0.12 * height;
        float flameShape = 1.0 - smoothstep(width * 0.44, width, abs(uv.x - 0.5 - wobble));
        float lick = valueNoise(vec2(uv.x * 9.0 + time * 1.9, height * 5.5 - time * 2.9));
        float topFade = 1.0 - smoothstep(0.78, 1.0, height);
        float baseFade = smoothstep(0.0, 0.13, height);
        alpha = clamp((flameShape * 0.84 + lick * 0.18) * topFade * baseFade, 0.0, 0.82);
        if (alpha < 0.035) {
            discard;
        }
    }
    if (smoke > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        float width = mix(0.20, 0.48, smoothstep(0.0, 1.0, height));
        float drift = (valueNoise(vec2(height * 2.7 + time * 0.18, uv.x * 3.4)) - 0.5) * 0.20;
        float smokeShape = 1.0 - smoothstep(width * 0.45, width, abs(uv.x - 0.5 - drift));
        float breakup = valueNoise(vec2(uv.x * 4.0 + time * 0.14, height * 5.3 - time * 0.22));
        alpha = clamp(smokeShape * smoothstep(0.03, 0.28, height) * (1.0 - smoothstep(0.74, 1.0, height)) * (0.10 + breakup * 0.20), 0.0, 0.24);
        if (alpha < 0.018) {
            discard;
        }
    }
    if (terrain > 0.5) {
        vec2 gradient = terrainGradient(worldPosition.xz);
        normal = normalize(normal + vec3(-gradient.x, 0.0, -gradient.y) * 0.32);
    }
    if (water > 0.5) {
        float swellA = sin(worldPosition.x * 0.38 + worldPosition.z * 0.17 + time * 0.78);
        float swellB = cos(worldPosition.z * 0.44 - worldPosition.x * 0.21 - time * 0.61);
        vec2 rippleA = vec2(
            sin(worldPosition.x * 2.6 + worldPosition.z * 1.2 + time * 1.9),
            cos(worldPosition.z * 3.0 - worldPosition.x * 0.75 - time * 1.55)
        );
        vec2 rippleB = vec2(
            sin(dot(worldPosition.xz, vec2(1.7, -2.2)) + time * 2.4),
            cos(dot(worldPosition.xz, vec2(-2.6, 1.3)) - time * 2.1)
        );
        normal = normalize(normal + vec3(rippleA * 0.021 + rippleB * 0.012 + vec2(swellA, swellB) * 0.018, 0.0).xzy);
    }
    vec3 viewDirection = normalize(cameraPosition - worldPosition);
    vec3 lightDirection = normalize(sunDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float moonDiffuse = max(dot(normal, normalize(moonDirection)), 0.0);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float materialRoughness = water > 0.5 ? 0.18 : lava > 0.5 || fire > 0.5 ? 0.18 : roughness;
    float specularPower = mix(128.0, 4.0, materialRoughness * materialRoughness);
    float specular = pow(max(dot(normal, halfDirection), 0.0), specularPower);
    vec3 moonHalfDirection = normalize(normalize(moonDirection) + viewDirection);
    float moonSpecular = pow(max(dot(normal, moonHalfDirection), 0.0), specularPower);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);
    float hemisphere = normal.y * 0.5 + 0.5;
    vec3 hemisphereLight = mix(ambientColor * 0.35, ambientColor * 1.35, hemisphere);
    float cavity = smoothstep(0.15, 0.85, normal.y) * mix(0.82, 1.0, hash(floor(worldPosition * 1.5)));
    float contactShade = mix(0.7, 1.0, smoothstep(0.0, 2.2, worldPosition.y)) * mix(0.82, 1.0, cavity);
    float shadow = shadowAmount(normal, lightDirection) * daylight;
    float cloudShadowNoise = terrainDetail(worldPosition.xz + vec2(time * 0.55, time * 0.12));
    float cloudShadow = mix(1.0, smoothstep(0.28, 0.78, cloudShadowNoise), cloudiness * 0.48);

    float surfaceVariation = mix(0.94, 1.06, hash(floor(worldPosition * 3.0)));
    float volcanicGlow = 0.0;
    float terrainSandAmount = 0.0;
    float terrainBasaltAmount = 0.0;
    float terrainWetness = 0.0;
    vec3 albedo = vertexColor;
    if (hasDiffuseTexture != 0) {
        albedo *= texture(diffuseTexture, uv).rgb;
    }
    if (terrain < 0.5 && water < 0.5 && lava < 0.5 && fire < 0.5 && smoke < 0.5) {
        albedo = objectMaterialDetail(albedo, worldPosition, normal);
    }
    if (terrain > 0.5) {
        float steep = smoothstep(0.58, 0.9, 1.0 - normal.y);
        float detail = terrainDetail(worldPosition.xz);
        float fine = valueNoise(worldPosition.xz * 5.5);
        float grains = valueNoise(worldPosition.xz * 18.0);
        float cells = cellular(worldPosition.xz * 0.72);
        float moss = smoothstep(0.42, 0.72, detail) * (1.0 - steep);
        float heightJitter = (detail - 0.5) * 0.8;
        float height = worldPosition.y + heightJitter;

        vec3 wetSand = mix(vec3(0.20, 0.22, 0.16), vec3(0.34, 0.32, 0.20), detail);
        vec3 beachSand = mix(vec3(0.43, 0.34, 0.20), vec3(0.68, 0.58, 0.36), detail);
        vec3 lowGrass = mix(vec3(0.12, 0.27, 0.055), vec3(0.28, 0.46, 0.11), detail);
        vec3 forestSoil = mix(vec3(0.15, 0.18, 0.065), vec3(0.29, 0.27, 0.10), detail);
        vec3 highGrass = mix(vec3(0.17, 0.27, 0.09), vec3(0.34, 0.40, 0.16), detail);
        vec3 mountainRock = mix(vec3(0.23, 0.23, 0.21), vec3(0.46, 0.44, 0.38), detail);

        albedo = wetSand;
        albedo = mix(albedo, beachSand, smoothstep(-0.55, 0.05, height));
        albedo = mix(albedo, lowGrass, smoothstep(0.05, 0.65, height));
        albedo = mix(albedo, forestSoil, smoothstep(1.35, 2.25, height));
        albedo = mix(albedo, highGrass, smoothstep(2.8, 4.0, height));
        albedo = mix(albedo, mountainRock, smoothstep(4.8, 6.5, height));
        albedo = mix(albedo, mountainRock, steep);

        vec3 earthTexture = triplanarSample(terrainEarthTexture, worldPosition, normal, 0.105);
        vec3 sandTexture = triplanarSample(terrainSandTexture, worldPosition, normal, 0.12);
        vec3 basaltTexture = triplanarSample(terrainBasaltTexture, worldPosition, normal, 0.09);
        float materialNoise = detail * 0.72 + valueNoise(worldPosition.xz * 0.035) * 0.28;
        float blendJitter = (materialNoise - 0.5) * 0.65;
        float sandWeight = (1.0 - smoothstep(-0.05 + blendJitter, 0.85 + blendJitter, height))
            * (1.0 - steep);
        float volcanoWeight = (1.0 - smoothstep(24.0, 47.0, length(worldPosition.xz)))
            * smoothstep(5.5 + blendJitter, 13.0 + blendJitter, height);
        float basaltWeight = max(steep * smoothstep(2.4 + blendJitter, 5.8 + blendJitter, height), volcanoWeight);
        float earthWeight = clamp(1.0 - max(sandWeight, basaltWeight), 0.0, 1.0);
        float materialTotal = max(earthWeight + sandWeight + basaltWeight, 0.0001);
        vec3 texturedTerrain = (earthTexture * earthWeight
            + sandTexture * sandWeight
            + basaltTexture * basaltWeight) / materialTotal;
        float macroVariation = valueNoise(worldPosition.xz * 0.018);
        texturedTerrain *= mix(vec3(0.82, 0.88, 0.78), vec3(1.10, 1.06, 0.98), macroVariation);
        albedo = mix(albedo, texturedTerrain, 0.72);
        terrainSandAmount = sandWeight / materialTotal;
        terrainBasaltAmount = basaltWeight / materialTotal;

        float stoneVeins = 1.0 - smoothstep(0.045, 0.16, cells);
        albedo = mix(albedo, albedo * vec3(0.58, 0.62, 0.60), stoneVeins * steep * 0.42);
        float grassFibers = smoothstep(0.73, 0.93, grains) * (1.0 - steep)
            * smoothstep(0.05, 0.8, height) * (1.0 - smoothstep(4.0, 5.3, height));
        albedo = mix(albedo, vec3(0.20, 0.43, 0.08), grassFibers * 0.22);
        albedo = mix(albedo, vec3(0.12, 0.30, 0.08), moss * 0.35 * smoothstep(0.5, 3.0, height));
        float shoreWetness = 1.0 - smoothstep(-0.62, -0.08, height);
        albedo = mix(albedo, albedo * vec3(0.48, 0.60, 0.57), shoreWetness * 0.55);
        terrainWetness = shoreWetness * terrainSandAmount;
        float terracePhase = abs(fract((height + 0.08) / 1.35) - 0.5) * 2.0;
        float terraceEdge = smoothstep(0.78, 0.98, terracePhase) * smoothstep(0.35, 1.4, height);
        albedo = mix(albedo, albedo * vec3(0.68, 0.72, 0.69), terraceEdge * (0.10 + steep * 0.28));
        float craterProximity = 1.0 - smoothstep(10.0, 31.0, length(worldPosition.xz));
        float hotCracks = smoothstep(0.72, 0.9, grains) * craterProximity * smoothstep(10.0, 18.0, height);
        albedo = mix(albedo, vec3(0.32, 0.035, 0.005), hotCracks * 0.75);
        volcanicGlow = hotCracks;
        albedo *= mix(0.90, 1.09, fine) * mix(0.96, 1.04, grains);
        float pathLike = smoothstep(0.42, 0.49, abs(sin(worldPosition.x * 0.025 + worldPosition.z * 0.003)));
        albedo = mix(albedo, albedo * vec3(0.82, 0.76, 0.64), pathLike * 0.08 * (1.0 - steep));
    }
    if (lava > 0.5) {
        vec2 centeredUv = uv - vec2(0.5);
        float radial = clamp(length(centeredUv) * 2.0, 0.0, 1.0);
        vec2 flowDirection = normalize(vec2(0.72, -0.38));
        float directional = sin(dot(worldPosition.xz, flowDirection) * 0.62 - time * 1.15);
        float counterFlow = sin(dot(worldPosition.xz, vec2(-flowDirection.y, flowDirection.x)) * 1.35 + time * 0.42);
        float flowA = valueNoise(worldPosition.xz * 0.23 + vec2(time * 0.25, -time * 0.16));
        float flowB = valueNoise(worldPosition.xz * 0.95 - vec2(time * 0.18, time * 0.30));
        float flowC = valueNoise(worldPosition.xz * 3.8 + vec2(time * 0.07, -time * 0.09));
        float ropy = smoothstep(0.45, 0.88, abs(directional * 0.62 + counterFlow * 0.22 + flowB * 0.38));
        float crust = smoothstep(0.58 + (flowC - 0.5) * 0.18, 0.98, radial);
        crust = clamp(crust + smoothstep(0.76, 0.96, ropy) * 0.18 * (1.0 - smoothstep(0.0, 0.45, radial)), 0.0, 1.0);
        float hotCore = (1.0 - smoothstep(0.24, 0.82, radial)) * smoothstep(0.40, 0.84, flowA * 0.58 + flowB * 0.28 + ropy * 0.22);
        float coolingCracks = smoothstep(0.48, 0.76, flowA * 0.34 + flowB * 0.30 + flowC * 0.22 + ropy * 0.18) * crust;
        float moltenChannels = smoothstep(0.50, 0.84, flowA * 0.52 + flowB * 0.30 + ropy * 0.30) * (1.0 - crust * 0.66);
        vec3 coldCrust = mix(vec3(0.018, 0.016, 0.014), vec3(0.13, 0.045, 0.023), flowC);
        vec3 warmCrust = vec3(0.36, 0.050, 0.014);
        vec3 molten = mix(vec3(0.88, 0.12, 0.004), vec3(1.0, 0.68, 0.070), hotCore);
        albedo = mix(molten, coldCrust, crust * 0.86);
        albedo = mix(albedo, warmCrust, coolingCracks * 0.58);
        albedo = mix(albedo, vec3(1.0, 0.40, 0.020), moltenChannels * 0.62);
        volcanicGlow = max(hotCore * 1.04 + moltenChannels * 0.82 + coolingCracks * 0.24, 0.0);
        surfaceVariation = 1.0;
    }
    if (fire > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        float sideFalloff = 1.0 - smoothstep(0.02, 0.48, abs(uv.x - 0.5));
        float lick = valueNoise(vec2(uv.x * 8.5 + time * 1.6, uv.y * 6.5 - time * 2.1));
        float core = smoothstep(0.32, 0.88, lick + (1.0 - height) * 0.38 + sideFalloff * 0.28);
        float pulse = 0.78 + sin(time * 9.0 + worldPosition.x * 12.0 + worldPosition.z * 9.0) * 0.22;
        vec3 outerFlame = mix(vec3(0.78, 0.035, 0.002), vec3(1.0, 0.28, 0.025), height);
        vec3 innerFlame = mix(vec3(1.0, 0.58, 0.10), vec3(1.0, 0.94, 0.42), sideFalloff);
        albedo = mix(outerFlame, innerFlame, core);
        surfaceVariation = pulse;
        volcanicGlow = max(volcanicGlow, core);
    }
    if (smoke > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        float cool = smoothstep(0.0, 1.0, height);
        float grain = valueNoise(vec2(uv.x * 6.5 - time * 0.12, uv.y * 5.5 + time * 0.08));
        albedo = mix(vec3(0.31, 0.28, 0.24), vec3(0.56, 0.55, 0.52), cool);
        albedo *= 0.78 + grain * 0.22;
        surfaceVariation = 1.0;
    }
    vec3 color = albedo * surfaceVariation * (hemisphereLight + sunColor * diffuse * daylight * (1.0 - shadow) * cloudShadow + moonColor * moonDiffuse * nightFactor) * contactShade;
    float materialSpecular = water > 0.5
        ? 0.8
        : terrain > 0.5
            ? mix(0.08, 0.52, terrainWetness) + terrainBasaltAmount * 0.08
            : specularStrength;
    color += sunColor * specular * materialSpecular * (1.0 - shadow);
    color += moonColor * moonSpecular * nightFactor * materialSpecular * 0.75;
    color += ambientColor * rim * 0.2;
    color += emissiveColor;
    color += vec3(1.0, 0.16, 0.008) * volcanicGlow * (1.6 + sin(time * 1.3) * 0.25);
    for (int index = 0; index < 4; ++index) {
        if (index >= fireLightCount) {
            break;
        }
        float fireDistance = length(worldPosition - fireLightPositions[index]);
        float firePulse = 0.82 + sin(time * 8.7 + float(index) * 1.9) * 0.12 + sin(time * 15.3) * 0.06;
        float fireVisibility = mix(1.0, 0.12, daylight);
        float fireLight = exp(-fireDistance * fireLightFalloffs[index]) * firePulse * fireVisibility * fireLightIntensities[index];
        color += fireLightColors[index] * fireLight * (terrain > 0.5 ? 0.92 : 0.52);
    }
    if (lava > 0.5) {
        vec2 centeredUv = uv - vec2(0.5);
        float radial = clamp(length(centeredUv) * 2.0, 0.0, 1.0);
        float crust = smoothstep(0.56, 0.96, radial);
        float pulse = 0.82 + sin(time * 1.45 + worldPosition.x * 0.19 + worldPosition.z * 0.13) * 0.18
            + sin(time * 2.25 + radial * 7.0) * 0.06;
        color = albedo * mix(4.3 + pulse * 2.35, 0.70 + pulse * 0.28, crust);
        color += vec3(1.0, 0.19, 0.015) * volcanicGlow * 2.15;
    }
    if (fire > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        float fadeTip = 1.0 - smoothstep(0.76, 1.0, height);
        float pulse = 0.85 + sin(time * 11.0 + worldPosition.x * 8.0) * 0.15;
        float baseHeat = 1.0 - smoothstep(0.0, 0.95, height);
        color = albedo * (3.8 + pulse * 3.2) * (0.28 + fadeTip * 0.72);
        color += vec3(1.0, 0.28, 0.035) * rim * 1.2;
        color += vec3(1.0, 0.12, 0.01) * baseHeat * 0.95;
    }
    if (smoke > 0.5) {
        float height = clamp(uv.y, 0.0, 1.0);
        color = mix(albedo * (0.58 + daylight * 0.28), fogColor, 0.22 + height * 0.28);
    }

    if (water > 0.5) {
        float shimmer = sin(worldPosition.x * 0.18 + time * 0.7) * 0.026 + sin(worldPosition.z * 0.33 - time * 0.55) * 0.018;
        float crest = smoothstep(0.12, 0.29, worldPosition.y + 0.45);
        float islandRadius = length(vec2(worldPosition.x / 132.0, worldPosition.z / 112.0));
        float shoreBand = exp(-pow((islandRadius - 0.82) * 18.0, 2.0));
        float shoreBreak = smoothstep(0.35, 0.78, valueNoise(worldPosition.xz * 0.42 + vec2(time * 0.18, 0.0)));
        float distanceFade = smoothstep(12.0, 150.0, length(cameraPosition.xz - worldPosition.xz));
        float shallow = smoothstep(0.98, 0.72, islandRadius);
        float turbidity = valueNoise(worldPosition.xz * 0.06 + vec2(time * 0.015, -time * 0.01));
        vec3 deepWater = vec3(0.004, 0.050, 0.115);
        vec3 surfaceWater = vec3(0.018, 0.135 + shimmer, 0.245);
        vec3 shallowWater = vec3(0.105, 0.275, 0.245);
        vec3 sandyBottom = vec3(0.34, 0.30, 0.18);
        vec3 waterColor = mix(surfaceWater, shallowWater, shallow * 0.48);
        waterColor = mix(waterColor, sandyBottom, shallow * shoreBand * 0.28 * daylight);
        waterColor = mix(waterColor, deepWater, distanceFade * 0.68);
        waterColor = mix(waterColor, vec3(0.075, 0.125, 0.105), turbidity * shoreBand * 0.16);
        float fresnel = 0.025 + 0.52 * pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.6);
        vec3 reflectedLight = mix(sunColor, moonColor, nightFactor);
        vec3 reflection = mix(fogColor, reflectedLight, 0.08 + daylight * 0.11);
        vec3 foam = mix(vec3(0.30, 0.38, 0.40), vec3(0.78, 0.86, 0.82), daylight);
        float caustic = smoothstep(0.58, 0.92, valueNoise(worldPosition.xz * 2.1 + vec2(time * 0.21, -time * 0.14)))
            * shallow * (1.0 - distanceFade);
        color = mix(waterColor * mix(0.46, 0.96, daylight), reflection, fresnel * 0.48)
            + reflectedLight * mix(moonSpecular, specular, daylight) * 0.72;
        color += vec3(0.12, 0.34, 0.26) * caustic * daylight * 0.75;
        float foamAmount = crest * (0.04 + valueNoise(worldPosition.xz * 4.0 + time * 0.06) * 0.08)
            + shoreBand * shoreBreak * (0.18 + cloudiness * 0.08);
        color = mix(color, foam, clamp(foamAmount, 0.0, 0.42));
    }

    vec3 cameraToFragment = worldPosition - cameraPosition;
    float distanceToCamera = length(cameraToFragment);
    float horizontalDistance = length(cameraToFragment.xz);
    float heightFog = exp(-max(worldPosition.y + 2.0, 0.0) * 0.035);
    float horizonFog = smoothstep(95.0, 620.0, horizontalDistance);
    float weatherFog = 1.0 - exp(-fogDensity * horizontalDistance);
    float lowMist = heightFog * smoothstep(18.0, 170.0, horizontalDistance) * (0.05 + cloudiness * 0.14);
    float fog = clamp(weatherFog * (0.34 + horizonFog * 0.72) + lowMist, 0.0, 0.78 + cloudiness * 0.12);
    color = mix(color, fogColor, fog);
    fragmentColor = vec4(color, alpha);
}
