#version 450

layout(location = 0) in vec3 fragTexCoord;

layout(binding = 1) uniform samplerCube skyboxSampler;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(skyboxSampler, fragTexCoord);
}