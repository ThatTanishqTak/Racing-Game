// Debug lines on the global root signature: the vertices sit in the upload ring behind a transient descriptor

#include "ShaderInterop.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);

struct LineVertex
{
    float3 Position;
    uint Color; // RGBA8, R in the low byte, linear
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
};

float4 UnpackColor(uint packed)
{
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF) / 255.0;
}

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    StructuredBuffer<LineVertex> l_Vertices = ResourceDescriptorHeap[Draw.VertexBufferIndex];
    const LineVertex l_Vertex = l_Vertices[vertexId];

    VertexOutput l_Output;
    l_Output.Position = mul(float4(l_Vertex.Position, 1.0), Frame.ViewProjection);
    l_Output.Color = UnpackColor(l_Vertex.Color);

    return l_Output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    return input.Color;
}