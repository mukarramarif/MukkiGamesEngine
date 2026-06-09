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

struct RayTracingVertex
{
    vec4 position;
    vec4 normal;
};

struct PrimitiveInfo
{
    uint firstIndex;
    uint indexCount;
    uint pad0;
    uint pad1;
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

    payload.normal = normal;
    payload.color = vec3(0.9);
}
