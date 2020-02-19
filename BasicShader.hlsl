Texture2D<float4> tex;
SamplerState smp;

struct Output
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer cbuff0 {
	matrix world;
	matrix viewproj;
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

	output.svpos = mul(mul(position, world), viewproj);
	normal.w = 0.0;
	output.normal = mul(normal, world);
	output.uv = uv;

	return output;
}

float4 BasicPS(Output input) : SV_TARGET
{
	float3 light = normalize(float3(1.0, -1.0, 1.0));
	float brightness = dot(-light, input.normal);

	return float4(brightness, brightness, brightness, 1.0);
}
