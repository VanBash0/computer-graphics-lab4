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

    float gScreenHeight;
    float2 gCarPos;
    float gEnableShadertoy;
    float gCarVignetteIntensity;
    
    float gPixelateSpeed;
    float gPixelateAmplitude;
    float gMaxPixelSize;
    float gEnablePixelate;
    float gTotalTime;
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

float3 applyVignette(float2 uv, float3 color, float intensity)
{
    float2 coord = 2.0f * uv - 1.0f;
    float dist = length(coord);
    float vignette = 1.0f - smoothstep(gVignetteInnerRadius, gVignetteOuterRadius, dist) * intensity;
    return color * vignette;
}

static const float2 CAR_BODY_SIZE = float2(0.045f, 0.12f);
static const float2 WHEEL_SIZE = float2(0.06f, 0.015f);
static const float2 FRONT_WHEEL_SIZE = float2(0.06f, 0.0225f);
static const float3 ROAD_COLOR = float3(0.15f, 0.15f, 0.15f);
static const float3 CAR_COLOR = float3(0.195f, 0.78f, 0.7f);

float sdRect(float2 p, float2 size)
{
    float2 d = abs(p) - size;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float sdCircle(float2 p, float r)
{
    return length(p) - r;
}

float sdCar(float2 p)
{
    float carBody = sdRect(p, CAR_BODY_SIZE);
    float wheel1 = sdRect(p + float2(0.0f, CAR_BODY_SIZE.y * 0.75f), WHEEL_SIZE);
    float wheel2 = sdRect(p + float2(0.0f, CAR_BODY_SIZE.y * 0.35f), WHEEL_SIZE);
    float wheel3 = sdRect(p - float2(0.0f, CAR_BODY_SIZE.y * 0.5f), FRONT_WHEEL_SIZE);
    return min(carBody, min(wheel1, min(wheel2, wheel3)));
}

float4 getShadertoyGameColor(float2 uv)
{
    float2 uv_ = uv;
    uv = applyBarrelDistortion(uv);
    float2 shaderUv = float2(uv.x, 1.0f - uv.y);
    float2 carPos = float2(gCarPos.x, 1.0f - gCarPos.y);
    float aspectRatio = gScreenWidth / gScreenHeight;
    shaderUv.x *= aspectRatio;
    carPos.x *= aspectRatio;

    float distCar = sdCar(shaderUv - carPos);
    float3 col = lerp(CAR_COLOR, ROAD_COLOR, step(0.0f, distCar));
    col = applyApertureGrille(uv_, col);
    col = applyVignette(uv_, col, gCarVignetteIntensity);

    return float4(col, 1.0f);
}

float2 applyPixelate(float2 uv)
{
    float pixelSize = max(floor(gPixelateSpeed * sin(gPixelateAmplitude * gTotalTime) - gPixelateSpeed + gMaxPixelSize), 1.0f);
    float2 fragCoord = float2(uv.x * gScreenWidth, uv.y * gScreenHeight);
    uv = floor(fragCoord / pixelSize) * pixelSize;
    uv.x /= gScreenWidth;
    uv.y /= gScreenHeight;
    return uv;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;

    if (gEnableDistortion > 0.5f)
    {
        uv = applyBarrelDistortion(uv);
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        {
            if (gEnableShadertoy)
            {
                return getShadertoyGameColor(pin.TexC);
            }
            else
            {
                return float4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
    }
    
    float2 uvAfterDistortion = uv;
    
    if (gEnablePixelate > 0.5f)
    {
        uv = applyPixelate(uv);
    }

    float4 color = gInputColor.Sample(gSampler, uv);

    if (gEnableGammaCorrection > 0.5f)
    {
        color.rgb = applyGammaCorrection(color.rgb);
    }

    if (gEnableApertureGrille > 0.5f)
    {
        color.rgb = applyApertureGrille(uvAfterDistortion, color.rgb);
    }

    if (gEnableVignette > 0.5f)
    {
        color.rgb = applyVignette(pin.TexC, color.rgb, gVignetteIntensity);
    }

    return color;
}
