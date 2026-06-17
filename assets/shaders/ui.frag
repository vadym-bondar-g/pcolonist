#version 330 core

in vec2 localPosition;
out vec4 fragmentColor;

uniform vec4 color;
uniform vec4 rect;
uniform vec4 viewportRadius;
uniform int useTexture;
uniform sampler2D glyphTexture;

void main() {
    if (useTexture != 0) {
        float alpha = texture(glyphTexture, localPosition).r;
        fragmentColor = vec4(color.rgb, color.a * alpha);
        return;
    }

    float radius = viewportRadius.z;
    if (radius > 0.0) {
        vec2 point = localPosition * rect.zw;
        vec2 centered = abs(point - rect.zw * 0.5) - (rect.zw * 0.5 - vec2(radius));
        float distance = length(max(centered, 0.0)) - radius;
        float alpha = 1.0 - smoothstep(-1.0, 1.0, distance);
        fragmentColor = vec4(color.rgb, color.a * alpha);
    } else {
        fragmentColor = color;
    }
}
