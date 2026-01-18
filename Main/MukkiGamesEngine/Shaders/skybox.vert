#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) out vec3 fragTexCoord;

void main() {
    fragTexCoord = inPosition;
    vec4 pos = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    // Set z = w so depth is always 1.0 (furthest possible)
    gl_Position = pos.xyww;
}