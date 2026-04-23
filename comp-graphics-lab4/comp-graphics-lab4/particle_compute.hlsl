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

cbuffer ParticleSimCB : register(b0)
{
    float3 gEmitterPosition;
    float3 gInitialVelocity;
    float gInitialSize;
    float4 gInitialColor;
    
    float gDeltaTime;
    float gTotalTime;
    
    float3 gCameraPosition;
    
    float gMinLifetime;
    float gMaxLifetime;
    
    uint gEmitCount;
    
    float3 gPadding;
}

RWStructuredBuffer<Particle> g_ParticlePool : register(u0);
AppendStructuredBuffer<uint> g_DeadListAppend : register(u1);
ConsumeStructuredBuffer<uint> g_DeadListConsume : register(t1);
RWStructuredBuffer<float2> g_SortList : register(u2);

float rand01(uint seed)
{
    uint x = seed * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return (float) (x & 0x00FFFFFFu) / 16777215.0f;
}

[numthreads(256, 1, 1)]
void EmitCS(uint3 dtid : SV_DispatchThreadID)
{
    uint spawnId = dtid.x;
    if (spawnId >= gEmitCount)
    {
        return;
    }
    
    uint index = g_DeadListConsume.Consume();

    float t = rand01(spawnId + asuint(gTotalTime * 1000.0f));
    float angle = t * 6.2831853f;
    float speed = 1.0f + rand01(spawnId * 17u + 3u) * 2.0f;
    
    Particle p;
    p.position = gEmitterPosition;
    p.velocity = gInitialVelocity + float3(cos(angle) * speed, rand01(spawnId * 31u + 7u) * 2.0f, sin(angle) * speed);
    p.age = 0.0f;
    p.lifetime = lerp(gMinLifetime, gMaxLifetime, rand01(spawnId * 13u + 11u));
    p.color = gInitialColor;
    p.size = gInitialSize;
    p.padding = 0.0f.xxx;
    
    
    g_ParticlePool[index] = p;
}

[numthreads(256, 1, 1)]
void SimulateCS(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= 65536)
    {
        return;
    }
    
    Particle p = g_ParticlePool[i];
    
    if (p.age >= p.lifetime)
    {
        g_DeadListAppend.Append(i);
        return;
    }
    
    p.position += p.velocity * gDeltaTime;
    p.age += gDeltaTime;
    
    g_ParticlePool[i] = p;
    
    float3 toCam = p.position - gCameraPosition;
    float distSq = dot(toCam, toCam);
    
    g_SortList[i] = float2((float)i, distSq);
}