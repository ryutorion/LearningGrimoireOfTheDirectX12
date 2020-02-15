Texture2D<float4> tex;
SamplerState smp;

struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer cbuff0 {
	matrix mat;
};

Output BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	Output output;

	output.svpos = mul(pos, mat);
	output.uv = uv;

	return output;
}

float4 BasicPS(Output input) : SV_TARGET
{
	return float4(tex.Sample(smp, input.uv));
}
