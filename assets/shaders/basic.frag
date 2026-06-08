#version 330 core

in vec3 vertexColor;
in vec3 worldPosition;
in vec3 worldNormal;
in vec4 lightSpacePosition;
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
uniform float water;
uniform float time;
uniform sampler2D shadowMap;
uniform int shadowsEnabled;

float hash(vec3 position) {
    return fract(sin(dot(position, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

float shadowAmount(vec3 normal, vec3 lightDirection) {
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

void main() {
    vec3 normal = normalize(worldNormal);
    vec3 viewDirection = normalize(cameraPosition - worldPosition);
    vec3 lightDirection = normalize(sunDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float moonDiffuse = max(dot(normal, normalize(moonDirection)), 0.0);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfDirection), 0.0), water > 0.5 ? 96.0 : 24.0);
    vec3 moonHalfDirection = normalize(normalize(moonDirection) + viewDirection);
    float moonSpecular = pow(max(dot(normal, moonHalfDirection), 0.0), water > 0.5 ? 110.0 : 32.0);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);
    float hemisphere = normal.y * 0.5 + 0.5;
    vec3 hemisphereLight = mix(ambientColor * 0.35, ambientColor * 1.35, hemisphere);
    float contactShade = mix(0.72, 1.0, smoothstep(0.0, 2.2, worldPosition.y));
    float shadow = shadowAmount(normal, lightDirection) * daylight;

    float surfaceVariation = mix(0.94, 1.06, hash(floor(worldPosition * 3.0)));
    vec3 color = vertexColor * surfaceVariation * (hemisphereLight + sunColor * diffuse * daylight * (1.0 - shadow) + moonColor * moonDiffuse * nightFactor) * contactShade;
    color += sunColor * specular * (water > 0.5 ? 0.8 : 0.18) * (1.0 - shadow);
    color += moonColor * moonSpecular * nightFactor * (water > 0.5 ? 0.65 : 0.1);
    color += ambientColor * rim * 0.2;

    if (water > 0.5) {
        float shimmer = sin(worldPosition.x * 0.25 + time) * 0.04 + sin(worldPosition.z * 0.42 - time * 0.8) * 0.025;
        vec3 waterColor = vec3(0.012, 0.16 + shimmer, 0.32);
        float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
        vec3 reflectedLight = mix(sunColor, moonColor, nightFactor);
        vec3 reflection = mix(fogColor, reflectedLight, 0.22);
        color = mix(waterColor * mix(0.3, 1.0, daylight), reflection, fresnel * 0.72)
            + reflectedLight * mix(moonSpecular, specular, daylight) * 1.2;
    }

    float distanceToCamera = length(cameraPosition - worldPosition);
    float fog = clamp(1.0 - exp(-fogDensity * distanceToCamera), 0.0, 0.94);
    color = mix(color, fogColor, fog);
    fragmentColor = vec4(color, 1.0);
}
