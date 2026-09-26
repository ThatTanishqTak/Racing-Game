#include "ShaderInterop.hlsli"
#include "Fullscreen.hlsli"
#include "Sky.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);
ConstantBuffer<EnvironmentPassConstants> Pass : register(b2);

FullscreenOutput VSMain(uint vertexId : SV_VertexID)
{
    return FullscreenVertex(vertexId);
}

// The direction a texel of a cube face looks along, in the D3D face order +X, -X, +Y, -Y, +Z, -Z with V running down the face
float3 FaceDirection(uint face, float2 texCoord)
{
    const float2 l_Coordinate = texCoord * 2.0 - 1.0;

    float3 l_Direction;
    switch (face)
    {
        case 0:
            l_Direction = float3(1.0, -l_Coordinate.y, -l_Coordinate.x);
            break;
        case 1:
            l_Direction = float3(-1.0, -l_Coordinate.y, l_Coordinate.x);
            break;
        case 2:
            l_Direction = float3(l_Coordinate.x, 1.0, l_Coordinate.y);
            break;
        case 3:
            l_Direction = float3(l_Coordinate.x, -1.0, -l_Coordinate.y);
            break;
        case 4:
            l_Direction = float3(l_Coordinate.x, -l_Coordinate.y, 1.0);
            break;
        default:
            l_Direction = float3(-l_Coordinate.x, -l_Coordinate.y, -1.0);
            break;
    }

    return normalize(l_Direction);
}

// Van der Corput radical inverse paired with a stratified first coordinate
float2 Hammersley(uint index, uint count)
{
    uint l_Bits = reversebits(index);

    return float2((float) index / (float) count, (float) l_Bits * 2.3283064365386963e-10);
}

// A tangent frame around the normal, so hemisphere samples can be rotated into world space
float3x3 TangentFrame(float3 normal)
{
    const float3 l_Up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    const float3 l_Tangent = normalize(cross(l_Up, normal));
    const float3 l_Bitangent = cross(normal, l_Tangent);

    return float3x3(l_Tangent, l_Bitangent, normal);
}

// GGX half vector for the given alpha, the same roughness squared the forward shader uses
float3 ImportanceSampleGgx(float2 xi, float alpha, float3x3 frame)
{
    const float l_Phi = 2.0 * k_Pi * xi.x;
    const float l_CosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    const float l_SinTheta = sqrt(1.0 - l_CosTheta * l_CosTheta);
    const float3 l_Half = float3(l_SinTheta * cos(l_Phi), l_SinTheta * sin(l_Phi), l_CosTheta);

    return normalize(mul(l_Half, frame));
}

float3 ImportanceSampleCosine(float2 xi, float3x3 frame)
{
    const float l_Phi = 2.0 * k_Pi * xi.x;
    const float l_CosTheta = sqrt(1.0 - xi.y);
    const float l_SinTheta = sqrt(xi.y);
    const float3 l_Direction = float3(l_SinTheta * cos(l_Phi), l_SinTheta * sin(l_Phi), l_CosTheta);

    return normalize(mul(l_Direction, frame));
}

// Schlick-GGX geometry term with the IBL remapping k = alpha / 2
float GeometrySmithIbl(float nDotV, float nDotL, float alpha)
{
    const float l_K = alpha * 0.5;
    const float l_ViewTerm = nDotV / (nDotV * (1.0 - l_K) + l_K);
    const float l_LightTerm = nDotL / (nDotL * (1.0 - l_K) + l_K);

    return l_ViewTerm * l_LightTerm;
}

float3 SampleSky(float3 direction)
{
    return EvaluateSky(direction, Frame.SunDirection.xyz, Frame.SunColor.rgb, Frame.SkyTint.rgb);
}

// One mip of the specular cube: the sky convolved with the GGX lobe of this mip's roughness, view and normal both
// along the reflection as in the split-sum approximation. Mip 0 uses one sample at roughness 0, which is the sky itself
float4 PSSpecular(FullscreenOutput input) : SV_Target
{
    const float3 l_Normal = FaceDirection(Pass.
    Target.x, input.TexCoord);
    const float3x3 l_Frame = TangentFrame(l_Normal);
    const float l_Alpha = Pass.
    Filter.x * Pass.
    Filter.x;
    const uint l_SampleCount = Pass.
    Target.y;

    float3 l_Sum = 0.0;
    float l_Weight = 0.0;
    for (uint l_Index = 0; l_Index < l_SampleCount; ++l_Index)
    {
        const float3 l_Half = ImportanceSampleGgx(Hammersley(l_Index, l_SampleCount), l_Alpha, l_Frame);
        const float3 l_Light = normalize(2.0 * dot(l_Normal, l_Half) * l_Half - l_Normal);
        const float l_NDotL = dot(l_Normal, l_Light);
        if (l_NDotL > 0.0)
        {
            l_Sum += SampleSky(l_Light) * l_NDotL;
            l_Weight += l_NDotL;
        }
    }

    return float4(l_Sum / max(l_Weight, 1e-4), 1.0);
}

// The irradiance cube stores the cosine-weighted hemisphere average, which is the irradiance over pi, so the forward shader multiplies it straight by the albedo
float4 PSIrradiance(FullscreenOutput input) : SV_Target
{
    const float3 l_Normal = FaceDirection(Pass.
    Target.x, input.TexCoord);
    const float3x3 l_Frame = TangentFrame(l_Normal);
    const uint l_SampleCount = Pass.
    Target.y;

    float3 l_Sum = 0.0;
    for (uint l_Index = 0; l_Index < l_SampleCount; ++l_Index)
    {
        l_Sum += SampleSky(ImportanceSampleCosine(Hammersley(l_Index, l_SampleCount), l_Frame));
    }

    return float4(l_Sum / (float) l_SampleCount, 1.0);
}

// Split-sum BRDF table: U is N.V, V is the roughness; R scales F0 and G adds to it
float2 PSBrdf(FullscreenOutput input) : SV_Target
{
    const float l_NDotV = max(input.TexCoord.x, 1e-3);
    const float l_Roughness = input.TexCoord.y;
    const float l_Alpha = l_Roughness * l_Roughness;
    const uint l_SampleCount = Pass.
    Target.y;

    const float3 l_View = float3(sqrt(1.0 - l_NDotV * l_NDotV), 0.0, l_NDotV);
    const float3x3 l_Frame = float3x3(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0));

    float2 l_Sum = 0.0;
    for (uint l_Index = 0; l_Index < l_SampleCount; ++l_Index)
    {
        const float3 l_Half = ImportanceSampleGgx(Hammersley(l_Index, l_SampleCount), l_Alpha, l_Frame);
        const float3 l_Light = 2.0 * dot(l_View, l_Half) * l_Half - l_View;
        const float l_NDotL = saturate(l_Light.z);
        const float l_NDotH = saturate(l_Half.z);
        const float l_VDotH = saturate(dot(l_View, l_Half));
        if (l_NDotL > 0.0)
        {
            const float l_Visibility = GeometrySmithIbl(l_NDotV, l_NDotL, l_Alpha) * l_VDotH / max(l_NDotH * l_NDotV, 1e-4);
            const float l_Fresnel = pow(1.0 - l_VDotH, 5.0);
            l_Sum += float2((1.0 - l_Fresnel) * l_Visibility, l_Fresnel * l_Visibility);
        }
    }

    return l_Sum / (float) l_SampleCount;
}