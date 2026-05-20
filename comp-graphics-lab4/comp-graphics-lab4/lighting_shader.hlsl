cbuffer cbPass : register(b0)
{
    float4x4 gInvViewProj;
    float4x4 gView;
    float4x4 gProj;
    float4x4 gShadowViewProj[4];
    float4 gShadowCascadeSplits;
    float3 gEyePosW;
    float gPadding;
    float4 gAmbientColor;
    float gExposure;
    float gEnableHdr;
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
Texture2DArray gShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

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

float calcShadow(float3 worldPos)
{
    float depth = distance(worldPos, gEyePosW);
    uint cascadeIndex = 0;
    if (depth > gShadowCascadeSplits.x)
        cascadeIndex = 1;
    if (depth > gShadowCascadeSplits.y)
        cascadeIndex = 2;
    if (depth > gShadowCascadeSplits.z)
        cascadeIndex = 3;
        
    float4 shadowPos = mul(float4(worldPos, 1.0f), gShadowViewProj[cascadeIndex]);
    shadowPos.xyz /= shadowPos.w;
    
    float2 shadowTexC = shadowPos.xy * 0.5f + 0.5f;
    shadowTexC.y = 1.0f - shadowTexC.y;

    float currentDepth = shadowPos.z;
    
    if (currentDepth > 1.0f)
        return 1.0f;
        
    float shadow = 0.0f;
    float width, height, elements;
    gShadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0f / float2(width, height);

    currentDepth -= 0.001f;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += gShadowMap.SampleCmpLevelZero(gShadowSampler,
                        float3(shadowTexC + offset, (float) cascadeIndex), currentDepth).r;
        }
    }

    return shadow / 9.0f;
}

float3 evaluateDirectionalLight(LightData light, float3 worldPos, float3 normalW)
{
    float3 lightVec = normalize(-light.Direction);
    float ndotl = max(dot(lightVec, normalW), 0.0f);
    
    float shadowFactor = calcShadow(worldPos);

    return light.Color * light.Intensity * ndotl * shadowFactor;
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
        return evaluateDirectionalLight(light, worldPos, normalW);
    }

    if (light.Type == LIGHT_TYPE_SPOT)
    {
        return evaluateSpotLight(light, worldPos, normalW);
    }

    return 0.0f;
}

float3 applyToneMapping(float3 color)
{
    return 1.0f - exp(-color * max(gExposure, 0.0001f));
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
    for (uint i = 0; i < min(gLightCount, MAX_LIGHTS); ++i)
    {
        lighting += evaluateLight(gLights[i], worldPos, normalW);
    }

    float3 litColor = albedo.rgb * lighting;
    if (gEnableHdr > 0.5f)
    {
        litColor = applyToneMapping(litColor);
    }
    return float4(litColor, albedo.a);
}