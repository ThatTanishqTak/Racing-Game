// One triangle that covers the whole viewport, for the sky, the environment bakes and the post pass

struct FullscreenOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

// Vertex ids 0, 1 and 2 land at the top left, far right and far bottom of clip space; the depth is 0, the far plane under reversed-Z
FullscreenOutput FullscreenVertex(uint vertexId)
{
    FullscreenOutput l_Output;
    l_Output.TexCoord = float2((vertexId << 1) & 2, vertexId & 2);
    l_Output.Position = float4(l_Output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    return l_Output;
}