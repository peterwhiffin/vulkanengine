/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

// objectSpace      →      worldSpace       →      cameraSpace      →       clipSpace         →         NDC space      →       viewportSpace
//            [Model matrix]         [View matrix]         [Projection matrix]    [perspective W division]      [remapping (-1; 1) to screen sizes]

struct VSOutput {
	float4 Pos : SV_POSITION;
	float2 UV;
};

//vk::binding(binding, set)
[[vk::binding(0, 0)]]
Texture2D<float4> texy;

[[vk::binding(1, 0)]]
SamplerState sampler;

[shader("vertex")]
VSOutput vertMain(uint vertex_id: SV_VertexID, uniform float4 color)
{
	float2 pos[3] = {
		float2(-1.0, -1.0),
		float2(3.0, -1.0),
		float2(-1.0, 3.0),
	};

	VSOutput output;
	output.Pos = float4(pos[vertex_id], 0.0, 1.0);
	output.UV = pos[vertex_id] * 0.5 + 0.5;
	return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input, uniform float4 color)
{
	float3 diffuse_color = texy.Sample(sampler, input.UV).rgb;
	// float3 final_color = mul(diffuse_color, color.rgb);
	// float3 final_color = mul(diffuse_color, float3(1.0, 1.0, 1.0));
	float3 final_color = diffuse_color;
	// final_color.r += 0.5;

	return float4(final_color.rgb, 1.0);
}
