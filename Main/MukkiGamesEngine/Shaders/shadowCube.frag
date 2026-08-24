#version 450

layout(location = 0) in vec3 worldPos;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 lightData;    // xyz = light position, w = 1 / farPlane
} push;

void main() {
    float dist = length(worldPos - push.lightData.xyz);
    gl_FragDepth = clamp(dist * push.lightData.w, 0.0, 1.0);
}
