cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);

    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normalW = normalize(pin.NormalW);

    float3 lightPos = float3(5.0f, 5.0f, -5.0f);
    float3 lightDir = normalize(lightPos - pin.PosW);
    float3 eyePos   = float3(0.0f, 0.0f, -5.0f);
    float3 viewDir  = normalize(eyePos - pin.PosW);

    float4 texColor = gDiffuseMap.Sample(gSampler, pin.TexC);
    float3 objectColor = texColor.rgb;

    float3 ambient = 0.2f * objectColor;

    float diff = max(dot(normalW, lightDir), 0.0f);
    float3 diffuse = diff * objectColor;

    float3 reflectDir = reflect(-lightDir, normalW);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 32.0f);
    float3 specular = spec * float3(1.0f, 1.0f, 1.0f);

    return float4(ambient + diffuse + specular, texColor.a);
}