#version 460

layout (location=0) out vec3 Dir;

layout (binding = 2) readonly uniform UniformBuffer { mat4 VP; } ubo;

const vec3 pos[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0),		// 0
	vec3( 1.0,-1.0, 1.0),		// 1
	vec3( 1.0, 1.0, 1.0),		// 2
	vec3(-1.0, 1.0, 1.0),		// 3

	vec3(-1.0,-1.0,-1.0),		// 4
	vec3( 1.0,-1.0,-1.0),		// 5
	vec3( 1.0, 1.0,-1.0),		// 6
	vec3(-1.0, 1.0,-1.0)		// 7
);

const int indices[36] = int[36](
	1, 0, 2, 3, 2, 0,	// front
	5, 1, 6, 2, 6, 1,	// right 
	6, 7, 5, 4, 5, 7,	// back
	0, 4, 3, 7, 3, 4,	// left
	5, 4, 1, 0, 1, 4,	// bottom
	2, 3, 6, 7, 6, 3	// top
);

void main() 
{
	int idx = indices[gl_VertexIndex];
	vec4 Pos = vec4(pos[idx], 1.0);
	vec4 WVP_Pos = ubo.VP * Pos;
	gl_Position = WVP_Pos.xyww;
	Dir = pos[idx].xyz;
}