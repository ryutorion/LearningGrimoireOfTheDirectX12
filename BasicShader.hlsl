Texture2D<float4> tex;
SamplerState smp;

struct Output
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer cbuff0 {
	matrix mat;
};

Output BasicVS(
	float4 position : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 bones : BONES,
	min16uint weight : WEIGHT
)
{
	Output output;

	output.svpos = mul(position, mat);
	output.normal = normal;
	output.uv = uv;

	return output;
}

float4 BasicPS(Output input) : SV_TARGET
{
	return float4(input.normal.xyz, 1.0);
}
