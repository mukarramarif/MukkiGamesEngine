#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_tracing_position_fetch : require

struct ProbePayload {
    float hitT;
    vec3 hitNormal;
};

layout(location = 0) rayPayloadInEXT ProbePayload payload;

void main()
{
    // Triangle vertex positions in object space (GL_EXT_ray_tracing_position_fetch)
    vec3 v0 = gl_HitTriangleVertexPositionsEXT[0];
    vec3 v1 = gl_HitTriangleVertexPositionsEXT[1];
    vec3 v2 = gl_HitTriangleVertexPositionsEXT[2];
    vec3 geoNormal = normalize(cross(v1 - v0, v2 - v0));

    // World-space geometric normal (inverse-transpose handles non-uniform
    // scale). Deliberately NOT flipped toward the ray (unlike rt.rchit):
    // the rgen needs the raw orientation for backface weighting.
    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    payload.hitNormal = normalize(normalMatrix * geoNormal);
    payload.hitT      = gl_HitTEXT;
}
