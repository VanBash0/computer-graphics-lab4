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

    float ambientStrength = 0.75f;
    float3 ambient = ambientStrength * albedo.rgb;

    return float4(ambient, albedo.a);
}
