uniform float4x4 u_Projection;
uniform float4x4 u_View;

struct VSInput  { float3 position : POSITION; };
struct VSOutput { float4 position : SV_Position; float3 worldPos : TEXCOORD0; };

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.worldPos = input.position;
    output.position = mul(u_Projection, mul(u_View, float4(input.position, 1.0)));
    return output;
}

Sampler2D u_EquirectangularMap;
static const float2 invAtan = float2(0.1591, 0.3183);

float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float2 uv    = SampleSphericalMap(normalize(input.worldPos));
    float3 color = u_EquirectangularMap.Sample(uv).rgb;
    return float4(color, 1.0);
}
