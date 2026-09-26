// Forward pass; today one hard-coded triangle pulled through the bindless heap, meshes and lighting follow

#include "ShaderInterop.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    // Vertex pulling: the mesh buffer is reached by descriptor index, so every mesh shares one pipeline with no input layout
    ByteAddressBuffer l_Vertices = ResourceDescriptorHeap[Draw.VertexBufferIndex];
    const Vertex l_Vertex = l_Vertices.Load < Vertex > (vertexId * k_VertexStride);

    VertexOutput l_Output;
    l_Output.Position = mul(float4(l_Vertex.Position, 1.0), Frame.ViewProjection);
    l_Output.Normal = l_Vertex.Normal;
    l_Output.TexCoord = l_Vertex.TexCoord;

    return l_Output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    // Texture coordinates as colour until materials arrive, so the pulled attributes are visible on screen
    return float4(input.TexCoord, saturate(1.0 - input.TexCoord.x - input.TexCoord.y), 1.0);
}