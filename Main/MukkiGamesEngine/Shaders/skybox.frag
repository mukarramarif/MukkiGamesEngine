#version 460

layout (location=0) in vec3 Dir;

layout (location=0) out vec4 out_Color;

layout(binding = 1) uniform samplerCube cubeSampler;

void main() 
{

	out_Color = texture(cubeSampler, Dir);
};