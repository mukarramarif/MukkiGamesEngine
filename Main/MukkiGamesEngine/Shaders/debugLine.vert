#version 460

// Debug line rendering (probe visualization, physics debug, etc.).
// Vertex: world-space position + color, transformed by a push-constant VP.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushBlock {
    mat4 viewProj;
} push;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = push.viewProj * vec4(inPos, 1.0);
    fragColor = inColor;
}
