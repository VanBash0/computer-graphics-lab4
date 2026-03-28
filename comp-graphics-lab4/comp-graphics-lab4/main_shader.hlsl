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

cbuffer cbPass : register(b1)
{
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float gPassPadding;
    float4 gAmbientColor;
};

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDisplacementMap : register(t2);
SamplerState gSampler : register(s0);

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
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float3 BitangentW : BINORMAL;
    float2 TexC : TEXCOORD;
};

struct TessControlPoint
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float3 BitangentL : BINORMAL;
    float2 TexC : TEXCOORD;
};

struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
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
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);
    vout.BitangentW = mul(vin.BitangentL, (float3x3) gWorld);
    vout.PosH = mul(posW, gWorldViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texC.xy;

    return vout;
}

TessControlPoint VS_Tess(VertexIn vin)
{
    TessControlPoint vout;
    vout.PosL = vin.PosL;
    vout.NormalL = vin.NormalL;
    vout.TangentL = vin.TangentL;
    vout.BitangentL = vin.BitangentL;

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texC.xy;
    return vout;
}

PatchTess PatchHS(InputPatch<TessControlPoint, 3> patch, uint patchId : SV_PrimitiveID)
{
    PatchTess pt;
    float3 patchCenterL = (patch[0].PosL + patch[1].PosL + patch[2].PosL) / 3.0f;
    float3 patchCenterW = mul(float4(patchCenterL, 1.0f), gWorld).xyz;
    float distanceToCamera = distance(patchCenterW, gEyePosW);

    const float nearDistance = 4.0f;
    const float farDistance = 30.0f;
    const float maxTessFactor = gMaxTessellationFactor;
    const float minTessFactor = 1.0f;

    float lod = saturate((distanceToCamera - nearDistance) / (farDistance - nearDistance));
    float tessFactor = lerp(maxTessFactor, minTessFactor, lod);
    
    pt.EdgeTess[0] = tessFactor;
    pt.EdgeTess[1] = tessFactor;
    pt.EdgeTess[2] = tessFactor;
    pt.InsideTess = tessFactor;
    return pt;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchHS")]
TessControlPoint HS(InputPatch<TessControlPoint, 3> patch, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    return patch[i];
}

[domain("tri")]
VertexOut DS(PatchTess patchTess, float3 bary : SV_DomainLocation, const OutputPatch<TessControlPoint, 3> patch)
{
    VertexOut vout;

    float3 posL =
        bary.x * patch[0].PosL +
        bary.y * patch[1].PosL +
        bary.z * patch[2].PosL;

    float3 normalL = normalize(
        bary.x * patch[0].NormalL +
        bary.y * patch[1].NormalL +
        bary.z * patch[2].NormalL);
    float3 tangentL = normalize(
        bary.x * patch[0].TangentL +
        bary.y * patch[1].TangentL +
        bary.z * patch[2].TangentL);
    float3 bitangentL = normalize(
        bary.x * patch[0].BitangentL +
        bary.y * patch[1].BitangentL +
        bary.z * patch[2].BitangentL);

    float2 texC =
        bary.x * patch[0].TexC +
        bary.y * patch[1].TexC +
        bary.z * patch[2].TexC;

    const float displacementScale = .4f;
    float displacement = gDisplacementMap.SampleLevel(gSampler, texC, 0).r;
    posL += normalL * (displacement * gDisplacementScale);

    float4 posW = mul(float4(posL, 1.0f), gWorld);
    vout.NormalW = mul(normalL, (float3x3) gWorld);
    vout.TangentW = mul(tangentL, (float3x3) gWorld);
    vout.BitangentW = mul(bitangentL, (float3x3) gWorld);
    vout.PosH = mul(posW, gWorldViewProj);
    vout.TexC = texC;

    return vout;
}

GBufferOut PS(VertexOut pin)
{
    GBufferOut gout;
    float3 normalW = normalize(pin.NormalW);
    float3 tangentW = normalize(pin.TangentW - dot(pin.TangentW, normalW) * normalW);
    float3 bitangentW = normalize(pin.BitangentW - dot(pin.BitangentW, normalW) * normalW);

    float3 normalTS = gNormalMap.Sample(gSampler, pin.TexC).xyz * 2.0f - 1.0f;
    float3 mappedNormalW = normalize(normalTS.x * tangentW + normalTS.y * bitangentW + normalTS.z * normalW);

    float4 texColor = gDiffuseMap.Sample(gSampler, pin.TexC);
    gout.Albedo = texColor;
    gout.Normal = float4(mappedNormalW * 0.5f + 0.5f, 1.0f);
    gout.Depth = pin.PosH.z;

    return gout;
}