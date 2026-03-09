cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
}

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PixelOut
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TexC = vin.TexC;
    return vout;
}

PixelOut PS(VertexOut pin)
{
    PixelOut pout;
    pout.Albedo = gDiffuseMap.Sample(gSampler, pin.TexC);
    pout.Normal = float4(normalize(pin.NormalW), 1.0f);
    return pout;
}