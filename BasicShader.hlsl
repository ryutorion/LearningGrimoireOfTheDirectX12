Texture2D<float4> tex;
Texture2D<float4> sph;
Texture2D<float4> spa;
Texture2D<float4> toon;
SamplerState smp;
SamplerState smpToon;

struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};

cbuffer SceneData
{
	matrix view;
	matrix proj;
	float4 eye;
};

cbuffer Transform
{
	matrix world;
};

cbuffer Material
{
	float4 diffuse;
	float4 specular;
	float4 ambient;
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

	position = mul(position, world);
	output.svpos = mul(mul(position, view), proj);
	output.pos = mul(position, view);
	normal.w = 0.0;
	output.normal = mul(normal, world);
	output.vnormal = mul(output.normal, view);
	output.uv = uv;
	output.ray = normalize(position.xyz - eye.xyz);

	return output;
}

float4 BasicPS(Output input) : SV_TARGET
{
	float3 light = normalize(float3(1.0, -1.0, 1.0));
	float3 lightColor = float3(1, 1, 1);

	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDiff = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float2 sphere_map_uv = (input.vnormal.xy + float2(1.0, -1.0)) * float2(0.5, -0.5);

	float4 texColor = tex.Sample(smp, input.uv);

	return max(
		saturate(
			toonDiff *
			diffuse *
			texColor *
			sph.Sample(smp, sphere_map_uv)
		) +
		saturate(spa.Sample(smp, sphere_map_uv) * texColor) +
		float4(specularB * specular.rgb, 1),
		texColor * ambient
	);
}
