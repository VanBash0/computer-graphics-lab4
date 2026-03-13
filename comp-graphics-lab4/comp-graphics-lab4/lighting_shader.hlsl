cbuffer cbPass : register(b0)
{
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float gPadding;
    float4 gAmbientColor;
};

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_SPOT = 2;
static const uint MAX_LIGHTS = 16;

struct LightData
{
    float3 Position;
    float Range;

    float3 Color;
    float Intensity;

    float3 Direction;
    uint Type;

    float3 Attenuation;
    float SpotAngle;
};

cbuffer cbLighting : register(b1)
{
    uint gLightCount;
    float3 gLightingPadding;
    LightData gLights[MAX_LIGHTS];
};

Texture2D gAlbedoMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDepthMap : register(t2);
SamplerState gSampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    float2 tex[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    vout.TexC = tex[vid];
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return vout;
}

float3 reconstructWorldPosition(float2 uv, float depth)
{
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clipPos = float4(ndcXY, depth, 1.0f);
    float4 worldPos = mul(clipPos, gInvViewProj);
    return worldPos.xyz / worldPos.w;
}

float computeAttenuation(LightData light, float distanceToLight)
{
    float constantTerm = light.Attenuation.x;
    float linearTerm = light.Attenuation.y * distanceToLight;
    float quadraticTerm = light.Attenuation.z * distanceToLight * distanceToLight;
    float denominator = max(constantTerm + linearTerm + quadraticTerm, 0.0001f);

    float rangeFade = saturate(1.0f - distanceToLight / max(light.Range, 0.0001f));
    return (1.0f / denominator) * rangeFade * rangeFade;
}

float3 evaluatePointLight(LightData light, float3 worldPos, float3 normalW)
{
    float3 toLight = light.Position - worldPos;
    float distanceToLight = length(toLight);
    if (distanceToLight >= light.Range)
    {
        return 0.0f;
    }

    float3 lightDir = toLight / max(distanceToLight, 0.0001f);
    float ndotl = saturate(dot(normalW, lightDir));
    if (ndotl <= 0.0f)
    {
        return 0.0f;
    }

    float attenuation = computeAttenuation(light, distanceToLight);
    return light.Color * light.Intensity * ndotl * attenuation;
}

float3 evaluateDirectionalLight(LightData light, float3 normalW)
{
    float3 lightDirection = normalize(light.Direction);
    float3 toLight = -lightDirection;
    float ndotl = saturate(dot(normalW, toLight));
    if (ndotl <= 0.0f)
    {
        return 0.0f;
    }

    return light.Color * light.Intensity * ndotl;
}

float3 evaluateSpotLight(LightData light, float3 worldPos, float3 normalW)
{
    float3 toLight = light.Position - worldPos;
    float distanceToLight = length(toLight);
    if (distanceToLight >= light.Range)
    {
        return 0.0f;
    }

    float3 lightDir = toLight / max(distanceToLight, 0.0001f);
    float ndotl = saturate(dot(normalW, lightDir));
    if (ndotl <= 0.0f)
    {
        return 0.0f;
    }

    float3 spotDirection = normalize(light.Direction);
    float3 lightToSurface = normalize(worldPos - light.Position);
    float cosTheta = dot(lightToSurface, spotDirection);
    float cosOuter = cos(light.SpotAngle * 0.5f);
    float cosInner = cos(light.SpotAngle * 0.35f);
    float spotFactor = saturate((cosTheta - cosOuter) / max(cosInner - cosOuter, 0.0001f));
    if (spotFactor <= 0.0f)
    {
        return 0.0f;
    }

    float attenuation = computeAttenuation(light, distanceToLight);
    return light.Color * light.Intensity * ndotl * attenuation * spotFactor * spotFactor;
}

float3 evaluateLight(LightData light, float3 worldPos, float3 normalW)
{
    if (light.Type == LIGHT_TYPE_POINT)
    {
        return evaluatePointLight(light, worldPos, normalW);
    }
    
    if (light.Type == LIGHT_TYPE_DIRECTIONAL)
    {
        return evaluateDirectionalLight(light, normalW);
    }

    if (light.Type == LIGHT_TYPE_SPOT)
    {
        return evaluateSpotLight(light, worldPos, normalW);
    }

    return 0.0f;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;
    float4 albedo = gAlbedoMap.Sample(gSampler, uv);

    float3 encodedNormal = gNormalMap.Sample(gSampler, uv).xyz;
    float3 normalW = normalize(encodedNormal * 2.0f - 1.0f);

    float depth = gDepthMap.Sample(gSampler, uv).r;
    float3 worldPos = reconstructWorldPosition(uv, depth);

    float3 lighting = gAmbientColor.rgb;
    [loop]
    for (uint i = 0; i < gLightCount; ++i)
    {
        lighting += evaluateLight(gLights[i], worldPos, normalW);
    }

    return float4(albedo.rgb * lighting, albedo.a);
}