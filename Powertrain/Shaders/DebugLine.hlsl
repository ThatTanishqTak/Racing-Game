// Debug lines

struct LineVertex
{
    float3 Position;
    uint Color; // RGBA8, R in the low byte, linear
};

// Matches Matrix4: row-major with row vectors, so points transform as v * M
cbuffer FrameConstants : register(b0)
{
    row_major float4x4 ViewProjection;
};

StructuredBuffer<LineVertex> Vertices : register(t0);

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
    const LineVertex l_Vertex = Vertices[vertexId];

    VertexOutput l_Output;
    l_Output.Position = mul(float4(l_Vertex.Position, 1.0), ViewProjection);
    l_Output.Color = UnpackColor(l_Vertex.Color);

    return l_Output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    return input.Color;
}