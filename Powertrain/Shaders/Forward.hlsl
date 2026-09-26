#include "ShaderInterop.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);

static const float k_Pi = 3.14159265;
static const float k_MinRoughness = 0.04;

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : POSITION0;
    float3 Normal : NORMAL;
    // XYZ world tangent, W bitangent sign
    float4 Tangent : TANGENT;
    float2 TexCoord : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    ByteAddressBuffer l_Vertices = ResourceDescriptorHeap[Draw.VertexBufferIndex];
    StructuredBuffer<InstanceData> l_Instances = ResourceDescriptorHeap[Frame.FrameInfo.y];

    const Vertex l_Vertex = l_Vertices.Load < Vertex > (vertexId * k_VertexStride);
    const InstanceData l_Instance = l_Instances[Draw.InstanceIndex + instanceId];
    const float3x3 l_Rotation = (float3x3) l_Instance.World;

    VertexOutput l_Output;
    l_Output.WorldPosition = mul(float4(l_Vertex.Position, 1.0), l_Instance.World).xyz;
    l_Output.Position = mul(float4(l_Output.WorldPosition, 1.0), Frame.ViewProjection);
    l_Output.Normal = normalize(mul(l_Vertex.Normal, l_Rotation));
    l_Output.Tangent = float4(normalize(mul(l_Vertex.Tangent, l_Rotation)), l_Vertex.TangentSign);
    l_Output.TexCoord = l_Vertex.TexCoord;

    return l_Output;
}

float DistributionGGX(float nDotH, float alpha)
{
    const float l_Alpha2 = alpha * alpha;
    const float l_Denominator = nDotH * nDotH * (l_Alpha2 - 1.0) + 1.0;

    return l_Alpha2 / (k_Pi * l_Denominator * l_Denominator);
}

// Height-correlated Smith visibility, already divided by 4 N.L N.V
float VisibilitySmithGGX(float nDotV, float nDotL, float alpha)
{
    const float l_Alpha2 = alpha * alpha;
    const float l_LambdaV = nDotL * sqrt(nDotV * nDotV * (1.0 - l_Alpha2) + l_Alpha2);
    const float l_LambdaL = nDotV * sqrt(nDotL * nDotL * (1.0 - l_Alpha2) + l_Alpha2);

    return 0.5 / max(l_LambdaV + l_LambdaL, 1e-5);
}

float3 FresnelSchlick(float vDotH, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - vDotH, 5.0);
}

// ACES fit by Narkowicz; lives here until the post pass at step 5 takes over
float3 TonemapAces(float3 color)
{
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float SampleSunShadow(float3 worldPosition, float3 normal, float nDotL)
{
    const uint l_CascadeCount = Frame.ShadowInfo.y;
    if (l_CascadeCount == 0)
    {
        return 1.0;
    }

    const float l_ViewDepth = -mul(float4(worldPosition, 1.0), Frame.View).z;
    const uint l_Cascade = (l_ViewDepth > Frame.ShadowSplits.x ? 1 : 0) + (l_ViewDepth > Frame.ShadowSplits.y ? 1 : 0) + (l_ViewDepth > Frame.ShadowSplits.z ? 1 : 0) + (l_ViewDepth > Frame.ShadowSplits.w ? 1 : 0);
    if (l_Cascade >= l_CascadeCount)
    {
        return 1.0;
    }

    // Grazing light needs more offset than light along the normal
    const float l_TexelWorldSize = Frame.ShadowTexelSizes[l_Cascade];
    const float3 l_Offset = normal * (l_TexelWorldSize * Frame.ShadowParams.x * (1.0 - nDotL));
    const float4 l_ShadowPosition = mul(float4(worldPosition + l_Offset, 1.0), Frame.ShadowMatrices[l_Cascade]);

    const float2 l_Uv = l_ShadowPosition.xy * float2(0.5, -0.5) + 0.5;
    if (any(l_Uv < 0.0) || any(l_Uv > 1.0))
    {
        return 1.0;
    }

    // Reversed-Z: the receiver is lit when its depth is at or above the stored depth, and the bias leans towards lit
    const float l_ReceiverDepth = l_ShadowPosition.z + Frame.ShadowParams.y;

    Texture2DArray<float> l_ShadowMap = ResourceDescriptorHeap[Frame.ShadowInfo.x];
    SamplerComparisonState l_ShadowSampler = SamplerDescriptorHeap[k_SamplerShadow];

    float l_Visibility = 0.0;
    [unroll]
    for (int l_Y = -1; l_Y <= 1; ++l_Y)
    {
        [unroll]
        for (int l_X = -1; l_X <= 1; ++l_X)
        {
            l_Visibility += l_ShadowMap.SampleCmpLevelZero(l_ShadowSampler, float3(l_Uv, l_Cascade), l_ReceiverDepth, int2(l_X, l_Y));
        }
    }
    l_Visibility /= 9.0;

    const float l_Fade = saturate((l_ViewDepth - Frame.ShadowParams.z) / Frame.ShadowParams.w);

    return lerp(l_Visibility, 1.0, l_Fade);
}

float4 PSMain(VertexOutput input) : SV_Target
{
    StructuredBuffer<MaterialData> l_Materials = ResourceDescriptorHeap[Frame.FrameInfo.z];
    const MaterialData l_Material = l_Materials[Draw.MaterialIndex];
    SamplerState l_Sampler = SamplerDescriptorHeap[k_SamplerAnisotropicWrap];

    Texture2D l_BaseColorTexture = ResourceDescriptorHeap[l_Material.BaseColorTexture];
    Texture2D l_MetallicRoughnessTexture = ResourceDescriptorHeap[l_Material.MetallicRoughnessTexture];
    Texture2D l_NormalTexture = ResourceDescriptorHeap[l_Material.NormalTexture];
    Texture2D l_EmissiveTexture = ResourceDescriptorHeap[l_Material.EmissiveTexture];

    const float4 l_BaseColor = l_Material.BaseColor * l_BaseColorTexture.Sample(l_Sampler, input.TexCoord);
    if ((l_Material.Flags & k_MaterialFlagAlphaMask) != 0)
    {
        clip(l_BaseColor.a - l_Material.AlphaCutoff);
    }

    // glTF packing: roughness in G, metallic in B; the factors multiply the texture
    const float4 l_MetallicRoughness = l_MetallicRoughnessTexture.Sample(l_Sampler, input.TexCoord);
    const float l_Roughness = clamp(l_Material.Roughness * l_MetallicRoughness.g, k_MinRoughness, 1.0);
    const float l_Metallic = saturate(l_Material.Metallic * l_MetallicRoughness.b);
    const float3 l_Emissive = l_Material.Emissive.rgb * l_EmissiveTexture.Sample(l_Sampler, input.TexCoord).rgb;

    // Tangent-space normal map; the bitangent sign comes from the vertex so mirrored UVs stay right
    const float3 l_VertexNormal = normalize(input.Normal);
    const float3 l_Tangent = normalize(input.Tangent.xyz - l_VertexNormal * dot(input.Tangent.xyz, l_VertexNormal));
    const float3 l_Bitangent = cross(l_VertexNormal, l_Tangent) * input.Tangent.w;
    const float3 l_TangentNormal = l_NormalTexture.Sample(l_Sampler, input.TexCoord).xyz * 2.0 - 1.0;
    const float3 l_Normal = normalize(l_TangentNormal.x * l_Tangent + l_TangentNormal.y * l_Bitangent + l_TangentNormal.z * l_VertexNormal);

    const float3 l_View = normalize(Frame.CameraPosition.xyz - input.WorldPosition);
    const float3 l_Light = Frame.SunDirection.xyz;
    const float3 l_Half = normalize(l_View + l_Light);

    const float l_NDotV = max(dot(l_Normal, l_View), 1e-4);
    const float l_NDotL = saturate(dot(l_Normal, l_Light));
    const float l_NDotH = saturate(dot(l_Normal, l_Half));
    const float l_VDotH = saturate(dot(l_View, l_Half));

    const float3 l_F0 = lerp(float3(0.04, 0.04, 0.04), l_BaseColor.rgb, l_Metallic);
    const float l_Alpha = l_Roughness * l_Roughness;

    const float3 l_Fresnel = FresnelSchlick(l_VDotH, l_F0);
    const float3 l_Specular = DistributionGGX(l_NDotH, l_Alpha) * VisibilitySmithGGX(l_NDotV, l_NDotL, l_Alpha) * l_Fresnel;
    const float3 l_Diffuse = (1.0 - l_Fresnel) * (1.0 - l_Metallic) * l_BaseColor.rgb / k_Pi;

    const float l_Shadow = SampleSunShadow(input.WorldPosition, l_VertexNormal, l_NDotL);

    float3 l_Color = (l_Diffuse + l_Specular) * Frame.SunColor.rgb * (l_NDotL * l_Shadow);

    // Flat ambient until the sky and IBL arrive at step 4; metals take it through F0 so they do not go black in shadow
    l_Color += Frame.AmbientColor.rgb * (l_BaseColor.rgb * (1.0 - l_Metallic) + l_F0 * l_Metallic);
    l_Color += l_Emissive;

    // Exposure then tonemap; the sRGB render target encodes on write
    l_Color = TonemapAces(l_Color * Frame.AmbientColor.w);

    return float4(l_Color, l_BaseColor.a);
}