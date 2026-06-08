#version 330 core

const vec2 vertices[6] = vec2[](
    vec2(0, 0), vec2(1, 0), vec2(1, 1),
    vec2(0, 0), vec2(1, 1), vec2(0, 1)
);

out vec2 localPosition;
uniform vec4 rect;
uniform vec4 viewportRadius;

void main() {
    localPosition = vertices[gl_VertexID];
    vec2 pixel = rect.xy + localPosition * rect.zw;
    vec2 clip = pixel / viewportRadius.xy * 2.0 - 1.0;
    clip.y = -clip.y;
    gl_Position = vec4(clip, 0.0, 1.0);
}
