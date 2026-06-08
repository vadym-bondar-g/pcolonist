#version 330 core

in vec2 localPosition;
out vec4 fragmentColor;

uniform vec4 color;
uniform vec4 rect;
uniform vec4 viewportRadius;

void main() {
    vec3 shadedColor = color.rgb * mix(1.08, 0.9, localPosition.y);
    float radius = viewportRadius.z;
    if (radius > 0.0) {
        vec2 point = localPosition * rect.zw;
        vec2 centered = abs(point - rect.zw * 0.5) - (rect.zw * 0.5 - vec2(radius));
        float distance = length(max(centered, 0.0)) - radius;
        float alpha = 1.0 - smoothstep(-1.0, 1.0, distance);
        fragmentColor = vec4(shadedColor, color.a * alpha);
    } else {
        fragmentColor = vec4(shadedColor, color.a);
    }
}
