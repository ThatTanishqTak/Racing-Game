#ifndef __cplusplus

struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float TangentSign;
    float2 TexCoord;
};
#endif

static const uint k_VertexStride = 48;
static const uint k_SamplerLinearWrap = 0;
static const uint k_SamplerLinearClamp = 1;
static const uint k_SamplerAnisotropicWrap = 2;
static const uint k_SamplerPointClamp = 3;
static const uint k_SamplerShadow = 4;
static const uint k_SamplerCount = 5;
static const uint k_MaterialFlagAlphaMask = 1;
static const uint k_ShadowCascadeCount = 4;

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

struct InstanceData
{
    row_major float4x4 World;
};

struct MaterialData
{
    float4 BaseColor;
    float4 Emissive;
    
    float Metallic;
    float Roughness;
    float AlphaCutoff;
    
    uint Flags;
    uint BaseColorTexture;
    uint MetallicRoughnessTexture;
    uint NormalTexture;
    uint EmissiveTexture;
};

struct ShadowPassConstants
{
    row_major float4x4 ViewProjection;
    uint4 Cascade;
};

struct FrameConstants
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 ViewProjection;
    
    float4 CameraPosition;
    float4 SunDirection;
    float4 SunColor;
    float4 AmbientColor;
    float4 ViewportSize;
    
    uint4 FrameInfo;

    row_major float4x4 ShadowMatrices[k_ShadowCascadeCount];
    float4 ShadowSplits;
    float4 ShadowTexelSizes;
    float4 ShadowParams;
    uint4 ShadowInfo;
};