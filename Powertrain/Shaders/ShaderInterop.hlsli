// Shared between HLSL and C++ (through Internal/Renderer/ShaderInterop.hpp), so the layouts cannot drift apart. Constant structs use only float4, float4x4 and uint members: those pack identically under HLSL cbuffer rules and C++.

#ifndef __cplusplus
// Mirrors Powertrain::Vertex in RenderTypes.hpp; pulled from a ByteAddressBuffer with Load<Vertex>, which packs tightly like C++
struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float TangentSign;
    float2 TexCoord;
};
#endif

// static const rather than constexpr because HLSL has no constexpr
static const uint k_VertexStride = 48;

// Root parameter 0: 8 root constants set per draw. InstanceIndex is the first instance of the draw; the shader adds SV_InstanceID
struct DrawConstants
{
    uint InstanceIndex;
    uint MaterialIndex;
    uint PassDataIndex;
    uint VertexBufferIndex;
    uint Reserved0;
    uint Reserved1;
    uint Reserved2;
    uint Reserved3;
};

// One visible mesh instance, written per frame into the upload ring; PreviousWorld joins when motion vectors need it
struct InstanceData
{
    row_major float4x4 World;
};

// Root parameter 1: root CBV written once per frame into the upload ring
struct FrameConstants
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 ViewProjection;
    // XYZ camera position, W near plane
    float4 CameraPosition;
    // XYZ unit direction towards the sun, W unused
    float4 SunDirection;
    // Width, height, 1 / width, 1 / height
    float4 ViewportSize;
    // X frame index, Y descriptor index of this frame's StructuredBuffer<InstanceData>, Z and W padding
    uint4 FrameInfo;
};