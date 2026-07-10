#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform LightSpaceMatrix {
    mat4 lightSpaceMatrix;
} push;

void main() {
    gl_Position = push.lightSpaceMatrix * vec4(inPosition, 1.0);
}
