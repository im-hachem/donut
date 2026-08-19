// Minimal shader used only to verify the Vulkan graphics-pipeline path (SPIR-V
// module -> pipeline -> draw -> read-back). Draws a full-screen gradient
// triangle from the vertex id, so it needs no vertex buffer, UBO, or sampler.

struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("vertex")]
VSOutput vertexMain(uint vid : SV_VertexID)
{
    float2 p = float2(float((vid << 1) & 2), float(vid & 2));
    VSOutput output;
    output.position = float4(p * 2.0 - 1.0, 0.0, 1.0);
    output.color    = float3(p, 0.5);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
