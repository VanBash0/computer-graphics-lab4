cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
    float4x4 gTexTransform;
    float gTotalTime;
    float gVertexAnimationEnabled;
    float gTextureAnimationEnabled;
    float gDisplacementScale;
    float gMaxTessellationFactor;
    float3 gPadding;
};

cbuffer cbShadowPass : register(b1)
{
    float4x4 gLightViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float3 BitangentL : BINORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

VertexOut ShadowVS(VertexIn vin)
{
    VertexOut vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gLightViewProj);
    
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texC.xy;

    return vout;
}

void ShadowPS(VertexOut pin)
{
    float alpha = gDiffuseMap.Sample(gSampler, pin.TexC).a;
    clip(alpha - .01f);
}