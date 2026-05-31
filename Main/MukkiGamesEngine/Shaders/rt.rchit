#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
    vec3 color;
    vec3 position;
    vec3 normal;
    int hit;
    int shadowRay;
};

layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.hit = 1;
    payload.position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.normal = normalize(gl_WorldRayDirectionEXT);
    payload.color = vec3(0.5, 0.2, 0.2);
}
