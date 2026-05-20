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
    float gGamma;
    float gEnableHdr;
    float gEnableGammaCorrection;
};

Texture2D gInputColor : register(t0);
SamplerState gSampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    float2 tex[4] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f)
    };

    vout.TexC = tex[vid];
    
    vout.PosH = float4(vout.TexC.x * 2.0f - 1.0f, 1.0f - vout.TexC.y * 2.0f, 0.0f, 1.0f);
    
    return vout;
}

float3 applyGammaCorrection(float3 color)
{
    return pow(saturate(color), 1.0f / max(gGamma, 0.0001f));
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 color = gInputColor.Sample(gSampler, pin.TexC);

    if (gEnableGammaCorrection > 0.5f)
    {
        color.rgb = applyGammaCorrection(color.rgb);
    }

    return color;
}
