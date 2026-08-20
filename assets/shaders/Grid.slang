uniform float4x4 u_ViewProjection;
uniform float4x4 u_Transform;
uniform float    u_GridSize;

struct VSInput  { float3 position : POSITION; };
struct VSOutput { float4 position : SV_Position; };

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    float3 scaledPosition = input.position * (u_GridSize / 50.0);
    output.position = mul(u_ViewProjection, mul(u_Transform, float4(scaledPosition, 1.0)));
    return output;
}

uniform float3 u_GridColor;
uniform float  u_GridAlpha;
uniform float3 u_CameraPos;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float dist       = length(u_CameraPos);
    float fadeFactor = 1.0 - clamp(dist / 100.0, 0.0, 0.8);

    float3 color = u_GridColor;
    float  alpha = u_GridAlpha * fadeFactor;

    float gridFade = 1.0 - clamp(dist / 50.0, 0.0, 0.9);
    alpha *= gridFade;

    return float4(color, alpha);
}
