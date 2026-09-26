// Sky pass: one fullscreen triangle at depth 0, so it lands only where nothing was drawn under reversed-Z

#include "ShaderInterop.hlsli"
#include "Fullscreen.hlsli"
#include "Sky.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);

FullscreenOutput VSMain(uint vertexId : SV_VertexID)
{
    return FullscreenVertex(vertexId);
}

float4 PSMain(FullscreenOutput input) : SV_Target
{
    // Unprojecting the pixel at depth 0 gives a point at infinity under the infinite projection, so the homogeneous result is a pure direction
    const float2 l_Clip = input.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0);
    const float4 l_World = mul(float4(l_Clip, 0.0, 1.0), Frame.InverseViewProjection);
    const float3 l_Direction = normalize(l_World.xyz);

    const float3 l_Sky = EvaluateSky(l_Direction, Frame.SunDirection.xyz, Frame.SunColor.rgb, Frame.SkyTint.rgb) + EvaluateSunDisc(l_Direction, Frame.SunDirection.xyz, Frame.SunColor.rgb);

    return float4(l_Sky, 1.0);
}