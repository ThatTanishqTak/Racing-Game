#include "ShaderInterop.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : POSITION0;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    ByteAddressBuffer l_Vertices = ResourceDescriptorHeap[Draw.VertexBufferIndex];
    StructuredBuffer<InstanceData> l_Instances = ResourceDescriptorHeap[Frame.FrameInfo.y];

    const Vertex l_Vertex = l_Vertices.Load < Vertex > (vertexId * k_VertexStride);
    const InstanceData l_Instance = l_Instances[Draw.InstanceIndex + instanceId];
    
    const float3 l_WorldPosition = mul(float4(l_Vertex.Position, 1.0), l_Instance.World).xyz;
    const float3 l_WorldNormal = normalize(mul(l_Vertex.Normal, (float3x3) l_Instance.World));

    VertexOutput l_Output;
    l_Output.Position = mul(float4(l_WorldPosition, 1.0), Frame.ViewProjection);
    l_Output.WorldPosition = l_WorldPosition;
    l_Output.Normal = l_WorldNormal;
    l_Output.TexCoord = l_Vertex.TexCoord;

    return l_Output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    const float3 l_Normal = normalize(input.Normal);
    const float l_Sun = saturate(dot(l_Normal, Frame.SunDirection.xyz));
    const float l_Ambient = 0.12 + 0.13 * saturate(l_Normal.y * 0.5 + 0.5);
    const float3 l_Albedo = float3(0.75, 0.75, 0.75);

    return float4(l_Albedo * (l_Sun * 0.85 + l_Ambient), 1.0);
}