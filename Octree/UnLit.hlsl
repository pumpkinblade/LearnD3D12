#include "Layout.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

float4 VS(VertexIn vin) : SV_POSITION
{
	return mul(mul(float4(vin.PosL, 1.0f), gWorld), gViewProj);
}

float4 PS() : SV_Target
{
	return gDiffuseAlbedo;
}
