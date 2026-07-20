/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

// objectSpace      →      worldSpace       →      cameraSpace      →       clipSpace         →         NDC space      →       viewportSpace
//            [Model matrix]         [View matrix]         [Projection matrix]    [perspective W division]      [remapping (-1; 1) to screen sizes]

struct VSInput {
	float3 Pos;
	float3 norm;
	float2 UV;
};

struct per_frame_uniforms {
	float4x4 proj;
	float4x4 view;
	float4 light_dir;
	float4 cam_pos;
};

struct entity {
	float4x4 model;
	float4x4 normal_matrix;
};

struct VSOutput {
	float4 Pos : SV_POSITION;
	float3 Norm;
	float2 UV;
	float3 frag_pos;
	nointerpolation float3 cam_pos;
	nointerpolation float3 light_dir;
};

struct push_const {
	per_frame_uniforms *pfu;
	uint32_t tex_index;
};

struct material {
	float specular_power;
};

//vk::binding(binding, set)
[[vk::binding(0, 0)]]
Texture2D<float4> tex[];

[[vk::binding(1, 0)]]
SamplerState sampler;

// [[vk::binding(1, 0)]]
// ConstantBuffer<material> materials[];

[[vk::binding(0, 1)]]
ConstantBuffer<entity> ebuf;

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform push_const pc)
{
	per_frame_uniforms *pfu = pc.pfu;
	VSOutput output;
	output.Pos = mul(pfu.proj, mul(pfu.view, mul(ebuf.model, float4(input.Pos.xyz, 1.0))));
	output.Norm = normalize(mul(float3x3(ebuf.normal_matrix), input.norm));
	output.UV = input.UV;
	output.frag_pos = mul(ebuf.model, float4(input.Pos, 1.0)).xyz;
	// output.frag_pos = mul(mul(pfu.view, ebuf.model), float4(input.Pos.xyz, 1.0)).xyz;
	output.cam_pos = pfu.cam_pos.xyz;
	output.light_dir = pfu.light_dir.xyz;
	return output;
}

float sd_circle(float2 v, float r)
{
	return length(v) - r;
}

[shader("fragment")]
float4 fragMain(VSOutput input, uniform push_const pc, float4 pos: SV_Position)
{
	float4 diffuse_color = tex[pc.tex_index].Sample(sampler, input.UV);

	if (diffuse_color.a < 0.3f)
		discard;

	float3 final_color = float3(0.0);

	float3 norm = normalize(input.Norm);
	float3 albedo = diffuse_color.rgb;
	float3 direction = normalize(-input.light_dir);
	float diff = max(dot(norm, direction), 0.0);

	final_color = albedo * diff;

	// float3 N = normalize(input.Norm);
	// float3 L = normalize(input.light_dir);
	// float3 V = normalize(-input.frag_pos);
	// float3 R = reflect(-L, N);
	// float3 diffuse = max(dot(N, L), 0.0025);
	// float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
	//
	// final_color = diffuse * diffuse_color.rgb + specular;

	return float4(final_color.rgb, 1.0);
}
