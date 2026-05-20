cbuffer cbPass : register(b0)
{
    float gGamma;
    float gEnableGammaCorrection;
    
    float gDistortionFactor;
    float gEnableDistortion;
    
    float gScreenWidth;
    float gApertureFrequency;
    float gEnableApertureGrille;
    
    float gVignetteInnerRadius;
    float gVignetteOuterRadius;
    float gVignetteIntensity;
    float gEnableVignette;
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

float2 applyBarrelDistortion(float2 uv)
{
    float2 coord = 2.f * uv - 1.f;
    float distSq = dot(coord, coord);
    coord *= 1.f + gDistortionFactor * distSq;

    return 0.5f * (coord + 1.f);
}

float3 applyApertureGrille(float2 uv, float3 color)
{
    float maskFrequency = gApertureFrequency * gScreenWidth;
    
    float3 mask;
    mask.r = sin(maskFrequency * uv.x) * 0.12f + 0.88f;
    mask.g = sin(maskFrequency * uv.x + 2.094f) * 0.12f + 0.88f;
    mask.b = sin(maskFrequency * uv.x + 4.188f) * 0.12f + 0.88f;
    
    return color * mask;
}

float3 applyVignette(float2 uv, float3 color)
{
    float2 coord = 2.0f * uv - 1.0f;
    float dist = length(coord);
    float vignette = 1.0f - smoothstep(gVignetteInnerRadius, gVignetteOuterRadius, dist) * gVignetteIntensity;
    return color * vignette;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;
    
    if (gEnableDistortion > 0.5f)
    {
        uv = applyBarrelDistortion(uv);
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        {
            return float4(0, 0, 0, 1);
        }
    }

    float4 color = gInputColor.Sample(gSampler, uv);

    if (gEnableGammaCorrection > 0.5f)
    {
        color.rgb = applyGammaCorrection(color.rgb);
    }
    
    if (gEnableApertureGrille > 0.5f)
    {
        color.rgb = applyApertureGrille(uv, color.rgb);
    }
    
    if (gEnableVignette > 0.5f)
    {
        color.rgb = applyVignette(pin.TexC, color.rgb);
    }

    return color;
}
