// Shadow pass: depth only, one cascade per invocation, same vertex pulling as the forward pass

#include "ShaderInterop.hlsli"

ConstantBuffer<DrawConstants> Draw : register(b0);
ConstantBuffer<FrameConstants> Frame : register(b1);
ConstantBuffer<ShadowPassConstants> Pass : register(b2);

float4 VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) : SV_Position
{
    ByteAddressBuffer l_Vertices = ResourceDescriptorHeap[Draw.VertexBufferIndex];
    StructuredBuffer<InstanceData> l_Instances = ResourceDescriptorHeap[Frame.FrameInfo.y];

    const Vertex l_Vertex = l_Vertices.Load < Vertex > (vertexId * k_VertexStride);
    const InstanceData l_Instance = l_Instances[Draw.InstanceIndex + instanceId];
    const float3 l_WorldPosition = mul(float4(l_Vertex.Position, 1.0), l_Instance.World).xyz;

    return mul(float4(l_WorldPosition, 1.0), Pass.ViewProjection);
}