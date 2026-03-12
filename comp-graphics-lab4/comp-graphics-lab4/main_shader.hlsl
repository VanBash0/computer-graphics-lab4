cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
    float4x4 gTexTransform;
    float gTotalTime;
    float3 gPadding;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL: POSITION;
    float3 NormalL: NORMAL;
    float2 TexC: TEXCOORD;
};

struct VertexOut
{
    float4 PosH: SV_POSITION;
    float3 NormalW: NORMAL;
    float2 TexC: TEXCOORD;
};

struct GBufferOut
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float Depth : SV_Target2;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.PosH = mul(posW, gWorldViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texC.xy;

    return vout;
}

GBufferOut PS(VertexOut pin)
{
    GBufferOut gout;
    float3 normalW = normalize(pin.NormalW);

    float4 texColor = gDiffuseMap.Sample(gSampler, pin.TexC);
    gout.Albedo = texColor;
    gout.Normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    gout.Depth = pin.PosH.z;

    return gout;
}
