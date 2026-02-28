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
    float3 PosW: POSITION;
    float3 NormalW: NORMAL;
    float2 TexC: TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    float speed = 4.f;
    float frequency = .5f;
    float amplitude = 2.f;
    
    float swing = sin(gTotalTime * speed + vin.PosL.y * frequency) * amplitude;
    float3 newPos = vin.PosL;
    newPos.x += swing * (vin.PosL.y * 0.1f);

    vout.PosW = mul(float4(newPos, 1.0f), gWorld).xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.PosH = mul(float4(newPos, 1.0f), gWorldViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = texC.xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normalW = normalize(pin.NormalW);

    float3 lightPos = float3(0.0f, 5.0f, -5.0f);
    float3 lightDir = normalize(lightPos - pin.PosW);
    float3 eyePos = float3(0.0f, 0.0f, -5.0f);
    float3 viewDir = normalize(eyePos - pin.PosW);

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