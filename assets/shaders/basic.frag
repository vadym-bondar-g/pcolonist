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
    vec3 detailed = albedo;
    detailed *= mix(0.84, 1.13, fibers * brown);
    detailed *= mix(0.82, 1.16, leafClusters * green);
    detailed *= mix(0.86, 1.08, smoothstep(0.12, 0.62, pores) * neutral);
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
    if (terrain > 0.5) {
        vec2 gradient = terrainGradient(worldPosition.xz);
        normal = normalize(normal + vec3(-gradient.x, 0.0, -gradient.y) * 0.32);
    }
    if (water > 0.5) {
        vec2 ripples = vec2(
            sin(worldPosition.x * 2.8 + worldPosition.z * 1.3 + time * 2.1),
            cos(worldPosition.z * 3.2 - worldPosition.x * 0.8 - time * 1.7)
        );
        normal = normalize(normal + vec3(ripples.x, 0.0, ripples.y) * 0.025);
    }
    vec3 viewDirection = normalize(cameraPosition - worldPosition);
    vec3 lightDirection = normalize(sunDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float moonDiffuse = max(dot(normal, normalize(moonDirection)), 0.0);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float materialRoughness = water > 0.5 ? 0.08 : lava > 0.5 ? 0.18 : roughness;
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
    if (terrain < 0.5 && water < 0.5 && lava < 0.5) {
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
        float flowA = valueNoise(worldPosition.xz * 0.32 + vec2(time * 0.22, -time * 0.13));
        float flowB = valueNoise(worldPosition.xz * 1.45 - vec2(time * 0.17, time * 0.28));
        float cracks = smoothstep(0.38, 0.72, flowA * 0.72 + flowB * 0.28);
        albedo = mix(vec3(0.32, 0.015, 0.002), vec3(1.0, 0.28, 0.008), cracks);
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
    if (lava > 0.5) {
        float pulse = 0.78 + sin(time * 1.7 + worldPosition.x * 0.19 + worldPosition.z * 0.13) * 0.22;
        color = albedo * (2.8 + pulse * 2.2);
    }

    if (water > 0.5) {
        float shimmer = sin(worldPosition.x * 0.25 + time) * 0.04 + sin(worldPosition.z * 0.42 - time * 0.8) * 0.025;
        float crest = smoothstep(0.12, 0.29, worldPosition.y + 0.45);
        float islandRadius = length(vec2(worldPosition.x / 132.0, worldPosition.z / 112.0));
        float shoreBand = exp(-pow((islandRadius - 0.82) * 18.0, 2.0));
        float shoreBreak = smoothstep(0.35, 0.78, valueNoise(worldPosition.xz * 0.42 + vec2(time * 0.18, 0.0)));
        float distanceFade = smoothstep(12.0, 150.0, length(cameraPosition.xz - worldPosition.xz));
        vec3 deepWater = vec3(0.006, 0.07, 0.18);
        vec3 surfaceWater = vec3(0.012, 0.19 + shimmer, 0.34);
        vec3 waterColor = mix(surfaceWater, deepWater, distanceFade * 0.7);
        float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(normal, viewDirection), 0.0), 5.0);
        vec3 reflectedLight = mix(sunColor, moonColor, nightFactor);
        vec3 reflection = mix(fogColor, reflectedLight, 0.18 + daylight * 0.12);
        vec3 foam = mix(vec3(0.28, 0.38, 0.44), vec3(0.78, 0.9, 0.92), daylight);
        color = mix(waterColor * mix(0.32, 1.0, daylight), reflection, fresnel * 0.82)
            + reflectedLight * mix(moonSpecular, specular, daylight) * 1.35;
        float foamAmount = crest * (0.08 + valueNoise(worldPosition.xz * 4.0 + time * 0.06) * 0.12)
            + shoreBand * shoreBreak * (0.32 + cloudiness * 0.15);
        color = mix(color, foam, clamp(foamAmount, 0.0, 0.72));
    }

    float distanceToCamera = length(cameraPosition - worldPosition);
    float fog = clamp(1.0 - exp(-fogDensity * distanceToCamera), 0.0, 0.94);
    color = mix(color, fogColor, fog);
    fragmentColor = vec4(color, 1.0);
}
