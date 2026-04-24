struct Particle
{
    float3 position;
    float age;
    float3 velocity;
    float lifetime;
    float4 color;
    float size;
    float3 padding;
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
    uint alive : ALIVE;
};

struct GSOut
{
    float4 positionH : SV_POSITION;
    float4 color : COLOR;
};

GSIn VS(VSIn vin)
{
    Particle p = gParticles[vin.particleId];
    GSIn output;
    output.position = p.position;
    output.size = p.size;
    output.color = p.color;
    output.alive = (p.age >= 0.0f && p.age < p.lifetime) ? 1u : 0u;
    
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
    
    GSOut outVertex;
    outVertex.color = gin[0].color;
    
    outVertex.positionH = mul(float4(corners[0], 1.0f), gView);
    outVertex.positionH = mul(outVertex.positionH, gProj);
    triStream.Append(outVertex);
    
    outVertex.positionH = mul(float4(corners[1], 1.0f), gView);
    outVertex.positionH = mul(outVertex.positionH, gProj);
    triStream.Append(outVertex);
    
    outVertex.positionH = mul(float4(corners[2], 1.0f), gView);
    outVertex.positionH = mul(outVertex.positionH, gProj);
    triStream.Append(outVertex);
    
    outVertex.positionH = mul(float4(corners[3], 1.0f), gView);
    outVertex.positionH = mul(outVertex.positionH, gProj);
    triStream.Append(outVertex);
    
    triStream.RestartStrip();
}

float4 PS(GSOut pin) : SV_Target
{
    return pin.color;
}