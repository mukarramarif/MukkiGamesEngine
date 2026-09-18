#version 460
#extension GL_EXT_ray_tracing : require

struct ProbePayload {
    float hitT;
    vec3 hitNormal;
};

layout(location = 0) rayPayloadEXT ProbePayload payload;

void main()
{
    payload.hitT = -1.0;
    payload.hitNormal = vec3(0.0);
}
