#include "Layout.hlsl"

struct VertexIn
{
	float3 PosL   : POSITION;
	float3 Normal : NORMAL;
	float2 TexC   : TEXCOORD;
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
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	VertexOut vout;
	vout.PosH = mul(posW, gViewProj);
	vout.PosW = posW.xyz;
	vout.TexC = vin.TexC;
	vout.NormalW = mul(vin.Normal, (float3x3)gWorldInvTranspose);

	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	float4 diffuseAlbedo = gDiffuseAlbedo * gDiffuseMap.Sample(gsamPointWrap, pin.TexC);

	pin.NormalW = normalize(pin.NormalW);

	float3 toEyeW = normalize(gEyePosW - pin.PosW);

	float4 ambient = gAmbientLight * diffuseAlbedo;

	float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;
	litColor.a = diffuseAlbedo.a;

	return litColor;
}