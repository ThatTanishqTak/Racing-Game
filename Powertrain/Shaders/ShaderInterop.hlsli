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

// Root parameter 0: 8 root constants set per draw
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

// Root parameter 1: root CBV written once per frame into the upload ring
struct FrameConstants
{
    row_major float4x4 ViewProjection;
    // Width, height, 1 / width, 1 / height
    float4 ViewportSize;
    // X is the frame index; the rest is padding until the sun and time arrive
    uint4 FrameInfo;
};