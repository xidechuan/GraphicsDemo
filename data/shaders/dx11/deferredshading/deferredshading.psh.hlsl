#include <common.h.hlsl>
#include <dscommon.h.hlsl>

Texture2D<float4> dataBlockMap1 : register(t1);
Texture2D<uint3> dataBlockMap2 : register(t2);

PS_OUTPUT_DS main(PS_INPUT_DS input)
{
	PS_OUTPUT_DS output;
	
	// clip empty fragments
	uint3 dataBlock2 = dataBlockMap2.Load(int3(input.position.xy, 0));
	int matId = dataBlock2.x & 0x0000ffff;
	clip(matId - 1);
	
	float3 nvr = normalize(input.viewRay);

	float4 dataBlock1 = dataBlockMap1.Load(int3(input.position.xy, 0));
	float3 worldPos = mul(unpackPosition(nvr, dataBlock1.x), viewInverse).xyz;
	float3 normal = unpackNormal(dataBlock1.yz);
	
	float4 diffTex = unpackColor(dataBlock2.y);
	float4 specularTex = unpackColor(dataBlock2.z);
	float specularPower = dataBlock1.w;
	
	// lighting
	float3 diffColor, specColor, ambColor;
	blinn(normal, worldPos, specularPower, diffColor, specColor, ambColor);
	
	float3 diffuse = diffTex * diffColor;
	float3 specular = specularTex.rgb * specColor;
	float3 ambient = diffTex * ambColor;
	
	output.color = float4(saturate(ambient + diffuse + specular), diffTex.a);
	
    return output;
}