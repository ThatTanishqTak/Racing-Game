// Post pass: the resolved scene colour into the swap chain with exposure and the ACES fit; the sRGB target encodes on write

#include "ShaderInterop.hlsli"
#include "Fullscreen.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);
ConstantBuffer<PostPassConstants> Pass : register(b2);

FullscreenOutput VSMain(uint vertexId : SV_VertexID)
{
    return FullscreenVertex(vertexId);
}

// ACES fit by Narkowicz
float3 TonemapAces(float3 color)
{
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float4 PSMain(FullscreenOutput input) : SV_Target
{
    Texture2D l_Scene = ResourceDescriptorHeap[Draw.PassDataIndex];

    // The resolved target is the swap chain's size, so the pixel maps one to one
    const float3 l_Color = l_Scene.Load(int3(input.Position.xy, 0)).rgb;

    return float4(TonemapAces(l_Color * Pass.
    Params.x),
    1.0);
}