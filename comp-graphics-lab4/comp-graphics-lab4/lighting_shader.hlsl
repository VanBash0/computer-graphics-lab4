cbuffer cbPass : register(b0)
{
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float gPadding;
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

    float2 tex[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    vout.TexC = tex[vid];
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;

    float4 albedo = gAlbedoMap.Sample(gSampler, uv);
    float3 normalW = normalize(gNormalMap.Sample(gSampler, uv).xyz * 2.0f - 1.0f);
    float depth = gDepthMap.Sample(gSampler, uv).r;

    float4 ndcPos = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y *= -1.0f;

    float4 worldPosH = mul(ndcPos, gInvViewProj);
    float3 worldPos = worldPosH.xyz / worldPosH.w;

    float3 lightPos = float3(0.0f, 5.0f, -5.0f);
    float3 lightDir = normalize(lightPos - worldPos);
    float3 viewDir = normalize(gEyePosW - worldPos);

    float3 ambient = 0.2f * albedo.rgb;
    float diff = max(dot(normalW, lightDir), 0.0f);
    float3 diffuse = diff * albedo.rgb;

    float3 reflectDir = reflect(-lightDir, normalW);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 32.0f);
    float3 specular = spec.xxx;

    return float4(ambient + diffuse + specular, albedo.a);
}
