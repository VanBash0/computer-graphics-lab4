cbuffer cbPass : register(b1)
{
    float4x4 gInvViewProj;
    float3 gEyePosW;
}

Texture2D gAlbedoMap : register(t1);
Texture2D gNormalMap : register(t2);
Texture2D gDepthMap : register(t3);
SamplerState gSampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 albedo = gAlbedoMap.Sample(gSampler, pin.TexC);

    float3 normalW = normalize(gNormalMap.Sample(gSampler, pin.TexC).xyz);
    float depth = gDepthMap.Sample(gSampler, pin.TexC).r;

    float x = pin.TexC.x * 2.0f - 1.0f;
    float y = (1.0f - pin.TexC.y) * 2.0f - 1.0f;

    float4 clipPos = float4(x, y, depth, 1.0f);

    float4 worldPos = mul(clipPos, gInvViewProj);
    worldPos /= worldPos.w;

    float3 posW = worldPos.xyz;

    float3 lightDir = normalize(float3(0.577f, 0.577f, -0.577f));
    float3 lightColor = float3(1.0f, 0.9f, 0.8f);

    float diff = max(dot(normalW, lightDir), 0.0f);
    float3 ambient = 0.15f * albedo.rgb;

    //return albedo;
    return float4(ambient + (albedo.rgb * lightColor * diff), 1.0f);
}