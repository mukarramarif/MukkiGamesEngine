#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;

void main() {
    // Hardcode a fullscreen triangle to eliminate any transformation issues
    vec3 positions[3] = vec3[](
        vec3(-1.0, -1.0, 0.0),
        vec3( 3.0, -1.0, 0.0),
        vec3(-1.0,  3.0, 0.0)
    );
    
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    fragColor = vec3(1.0, 0.0, 1.0); // Magenta
}