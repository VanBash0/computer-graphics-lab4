struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float lifetime;
    float4 color;
    float size;
    uint shapeId1;
    uint shapeId2;
    float padding;
};

cbuffer cbPass : register(b0)
{
    float4x4 gInvViewProj;
    float4x4 gView;
    float4x4 gProj;
    float3 gEyePosW;
    float gPad0;
    float4 gAmbientColor;
}

StructuredBuffer<Particle> gParticles : register(t0);

struct VSIn
{
    uint particleId : PARTICLEID;
};

struct GSIn
{
    float3 position : POSITION;
    float size : PSIZE;
    float4 color : COLOR;
    uint shapeId1 : SHAPEID1;
    uint shapeId2 : SHAPEID2;
    uint alive : ALIVE;
};

struct GSOut
{
    float4 positionH : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    uint shapeId1 : SHAPEID1;
    uint shapeId2 : SHAPEID2;
};

GSIn VS(VSIn vin)
{
    Particle p = gParticles[vin.particleId];
    GSIn output;
    output.position = p.position;
    output.size = p.size;
    output.color = p.color;
    output.alive = (p.age >= 0.0f && p.age < p.lifetime) ? 1u : 0u;
    output.shapeId1 = p.shapeId1;
    output.shapeId2 = p.shapeId2;
    
    return output;
}

[maxvertexcount(4)]
void GS(point GSIn gin[1], inout TriangleStream<GSOut> triStream)
{
    if (gin[0].alive == 0u)
    {
        return;
    }
    
    float3 right = normalize(float3(gView[0][0], gView[1][0], gView[2][0]));
    float3 up = normalize(float3(gView[0][1], gView[1][1], gView[2][1]));
    
    float halfSize = gin[0].size * 0.5f;
    float3 center = gin[0].position;
    
    float3 corners[4] =
    {
        center + (-right - up) * halfSize,
        center + (-right + up) * halfSize,
        center + (right - up) * halfSize,
        center + (right + up) * halfSize
    };
    
    float2 uvs[4] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f)
    };
    
    GSOut outputVertex;
    outputVertex.color = gin[0].color;
    outputVertex.shapeId1 = gin[0].shapeId1;
    outputVertex.shapeId2 = gin[0].shapeId2;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        outputVertex.uv = uvs[i];
        outputVertex.positionH = mul(float4(corners[i], 1.0f), gView);
        outputVertex.positionH = mul(outputVertex.positionH, gProj);
        triStream.Append(outputVertex);
    }
    
    triStream.RestartStrip();
}

float sdCircle(float2 p, float r)
{
    return length(p) - r;
}

float sdPentagon(float2 p, float r)
{
    const float3 k = float3(0.809016994, 0.587785252, 0.726542528);
    p.x = abs(p.x);
    p -= 2.0 * min(dot(float2(-k.x, k.y), p), 0.0) * float2(-k.x, k.y);
    p -= 2.0 * min(dot(float2(k.x, k.y), p), 0.0) * float2(k.x, k.y);
    p -= float2(clamp(p.x, -r * k.z, r * k.z), r);
    return length(p) * sign(p.y);
}

float sdBox(float2 p, float a)
{
    float2 d = abs(p) - float2(a, a);
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25f;
}



float calculateSDF(float2 uv, uint shapeId1, uint shapeId2)
{
    float2 p = uv * 2.0f - 1.0f;
    
    float radius = 0.45f;
    float2 offset1 = float2(-0.25f, -0.2f);
    float2 offset2 = float2(0.25f, 0.2f);
    
    float d1, d2;
    
    float2 p1 = p - offset1;
    switch (shapeId1)
    {
        case 0:
            d1 = sdCircle(p1, radius);
            break;
        case 1:
            d1 = sdPentagon(p1, radius);
            break;
        case 2:
            d1 = sdBox(p1, radius);
            break;
        default:
            d1 = 1.0f;
            break;
    }
    
    float2 p2 = p - offset2;
    switch (shapeId2)
    {
        case 0:
            d2 = sdCircle(p2, radius);
            break;
        case 1:
            d2 = sdPentagon(p2, radius);
            break;
        case 2:
            d2 = sdBox(p2, radius);
            break;
        default:
            d2 = 1.0f;
            break;
    }
    
    return smin(d1, d2, 0.5f);
}

float4 PS(GSOut pin) : SV_Target
{
    float d = calculateSDF(pin.uv, pin.shapeId1, pin.shapeId2);
    clip(-d);
    return pin.color;
}