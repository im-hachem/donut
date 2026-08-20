struct VSInput
{
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.texCoord = input.texCoord;
    return output;
}

Sampler2D u_ScreenTexture;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return u_ScreenTexture.Sample(input.texCoord);
}
