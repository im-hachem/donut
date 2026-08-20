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
uniform float2 u_Resolution;
uniform float  u_BlurStrength;
uniform float  u_GlowIntensity;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float2 texelSize = 1.0 / u_Resolution;
    float4 centerColor = u_ScreenTexture.Sample(input.texCoord);

    float brightness = (centerColor.r + centerColor.g + centerColor.b) / 3.0;
    float glowMask   = smoothstep(0.05, 0.3, brightness);

    float4 blurredColor = float4(0.0, 0.0, 0.0, 0.0);
    float  totalWeight  = 0.0;

    for (int x = -8; x <= 8; x++)
    {
        for (int y = -8; y <= 8; y++)
        {
            float2 offset = float2(x, y) * texelSize * u_BlurStrength;
            float4 sampleColor = u_ScreenTexture.Sample(input.texCoord + offset);

            float weight = exp(-float(x * x + y * y) / (2.0 * 8.0 * 8.0));
            blurredColor += sampleColor * weight;
            totalWeight  += weight;
        }
    }

    blurredColor /= totalWeight;

    float3 emissionColor = float3(1.0, 0.8, 0.6);
    float3 glowColor     = emissionColor * glowMask * u_GlowIntensity * 2.0;
    float3 auraColor     = blurredColor.rgb * glowMask * u_GlowIntensity * 0.8;
    float3 finalColor    = centerColor.rgb + glowColor + auraColor;

    return float4(finalColor, centerColor.a);
}
