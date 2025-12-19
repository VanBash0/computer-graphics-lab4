cbuffer cbPerObject : register(b0)
{
 float4x4 gWorldViewProj; 
};

void VS(float3 iPosL : POSITION, 
	float4 iColor : COLOR, 
	out float4 oPosH : SV_POSITION,
	out float4 oColor : COLOR)
{
	oPosH = mul(float4(iPosL, 1.0f), gWorldViewProj);
	oColor = iColor;
}

float4 PS(float4 posH : SV_POSITION,
	float4 color : COLOR) : SV_Target
{
	return pin.Color;
}