#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
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

struct RayTracingVertex
{
    vec4 position;
    vec4 normal;
    vec2 texCoord;
    float pad0;
    float pad1;
};

struct PrimitiveInfo
{
    uint firstIndex;
    uint indexCount;
    int textureIndex;
    float metallicFactor;
    float roughnessFactor;
    float baseColorR;
    float baseColorG;
    float baseColorB;
    float emissiveR;
    float emissiveG;
    float emissiveB;
};

struct MeshInfo
{
    uint primitiveOffset;
    uint primitiveCount;
    uint pad0;
    uint pad1;
};

layout(set = 0, binding = 3, std430) readonly buffer IndexBuffer
{
    uint indices[];
} indexBuffer;

layout(set = 0, binding = 4, std430) readonly buffer VertexBuffer
{
    RayTracingVertex vertices[];
} vertexBuffer;

layout(set = 0, binding = 5, std430) readonly buffer PrimitiveBuffer
{
    PrimitiveInfo primitives[];
} primitiveBuffer;

layout(set = 0, binding = 6, std430) readonly buffer MeshBuffer
{
    MeshInfo meshes[];
} meshBuffer;

layout(set = 0, binding = 8) uniform sampler2D textures[16];

hitAttributeEXT vec2 attribs;

void main()
{
    payload.hit = 1;
    payload.position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    uint meshIndex = gl_InstanceCustomIndexEXT;
    MeshInfo meshInfo = meshBuffer.meshes[meshIndex];
    uint primitiveIndex = meshInfo.primitiveOffset + gl_GeometryIndexEXT;
    PrimitiveInfo primInfo = primitiveBuffer.primitives[primitiveIndex];

    uint triIndex = primInfo.firstIndex + uint(gl_PrimitiveID) * 3u;
    uint i0 = indexBuffer.indices[triIndex + 0u];
    uint i1 = indexBuffer.indices[triIndex + 1u];
    uint i2 = indexBuffer.indices[triIndex + 2u];

    RayTracingVertex v0 = vertexBuffer.vertices[i0];
    RayTracingVertex v1 = vertexBuffer.vertices[i1];
    RayTracingVertex v2 = vertexBuffer.vertices[i2];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal.xyz * bary.x + v1.normal.xyz * bary.y + v2.normal.xyz * bary.z);
    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    normal = normalize(normalMatrix * normal);
    if (dot(normal, gl_WorldRayDirectionEXT) > 0.0)
    {
        normal = -normal;
    }

    vec2 uv = v0.texCoord * bary.x + v1.texCoord * bary.y + v2.texCoord * bary.z;
    int texIdx = primInfo.textureIndex;
    vec3 albedo;
    vec3 baseColor = vec3(primInfo.baseColorR, primInfo.baseColorG, primInfo.baseColorB);
    if (texIdx >= 0) {
        albedo = texture(textures[nonuniformEXT(texIdx)], uv).rgb * baseColor;
    } else {
        albedo = baseColor;
    }

    payload.normal = normal;
    payload.color = albedo;
    payload.metallic = primInfo.metallicFactor;
    payload.roughness = primInfo.roughnessFactor;
    payload.emissiveColor = vec3(primInfo.emissiveR, primInfo.emissiveG, primInfo.emissiveB);
}
