uniform float4x4 u_ViewProjection;
uniform float4x4 u_Transform;

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal   : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.worldPos = mul(u_Transform, float4(input.position, 1.0)).xyz;
    float3x3 normalMatrix = (float3x3)u_Transform;
    output.normal   = mul(normalMatrix, input.normal);
    output.position = mul(u_ViewProjection, float4(output.worldPos, 1.0));
    return output;
}

uniform float3 u_Color;
uniform float  u_Specular;
uniform float  u_Emission;
uniform float3 u_LightPos;
uniform float3 u_CameraPos;
uniform int    u_IsSelected;
uniform float3 u_OutlineColor;
uniform float  u_OutlineWidth;
SamplerCube    u_HDRIEnvironment;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float3 normal     = normalize(input.normal);
    float3 lightDir   = normalize(u_LightPos - input.worldPos);
    float3 viewDir    = normalize(u_CameraPos - input.worldPos);
    float3 reflectDir = reflect(-lightDir, normal);

    float3 hdriLight     = u_HDRIEnvironment.Sample(normal).rgb;
    float  hdriIntensity = 0.3;

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float3 diffuse  = u_Color * (diff + hdriIntensity * hdriLight);
    float3 specular = float3(u_Specular, u_Specular, u_Specular) * spec;
    float3 emission = u_Color * u_Emission;

    float3 result = diffuse + specular + emission;

    if (u_IsSelected > 0)
    {
        float ndotv       = max(dot(normal, viewDir), 0.0);
        float rim         = 1.0 - ndotv;
        float outlineMask = step(1.0 - u_OutlineWidth, rim);
        result = lerp(result, u_OutlineColor, outlineMask);
    }

    return float4(result, 1.0);
}
