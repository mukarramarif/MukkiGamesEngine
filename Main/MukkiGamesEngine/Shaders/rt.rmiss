#version 460
#extension GL_EXT_ray_tracing : require

struct Payload
{
    vec3 color;
    vec3 position;
    vec3 normal;
    int hit;
    int shadowRay;
    float metallic;
    float roughness;
    vec3 emissiveColor;
};

layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    if (payload.shadowRay == 1)
    {
        payload.hit = 0;
        return;
    }

    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = clamp(0.5 * (dir.y + 1.0), 0.0, 1.0);
    vec3 skyTop = vec3(0.6, 0.8, 1.0);
    vec3 skyBottom = vec3(0.02, 0.04, 0.08);
    payload.color = mix(skyBottom, skyTop, t);
    payload.hit = 0;
}
