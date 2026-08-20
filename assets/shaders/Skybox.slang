uniform float4x4 u_Projection;
uniform float4x4 u_View;

struct VSInput  { float3 position : POSITION; };
struct VSOutput { float4 position : SV_Position; float3 texCoords : TEXCOORD0; };

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.texCoords = input.position;
    float4 pos = mul(u_Projection, mul(u_View, float4(input.position, 1.0)));
    output.position = pos.xyww;   // force depth = 1 so the skybox stays behind everything
    return output;
}

SamplerCube u_Skybox;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return u_Skybox.Sample(input.texCoords);
}
