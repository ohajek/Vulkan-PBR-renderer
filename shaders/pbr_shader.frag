#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

// Scene bindings

layout (set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 model;
	mat4 view;
	vec3 camPos;
} ubo;

//layout (set = 0, binding = 1) uniform UBOParams {
//	vec4 lightDir;
//	float exposure;
//	float gamma;
//	float prefilteredCubeMipLevels;
//} uboParams;

//layout (set = 0, binding = 2) uniform samplerCube samplerIrradiance;
//layout (set = 0, binding = 3) uniform samplerCube prefilteredMap;
//layout (set = 0, binding = 4) uniform sampler2D samplerBRDFLUT;

// Material bindings

layout (set = 1, binding = 0) uniform sampler2D albedoMap;
layout (set = 1, binding = 1) uniform sampler2D normalMap;
layout (set = 1, binding = 2) uniform sampler2D aoMap;
layout (set = 1, binding = 3) uniform sampler2D metallicMap;
layout (set = 1, binding = 4) uniform sampler2D emissiveMap;

layout (push_constant) uniform Material {
	vec4 baseColorFactor;
	float hasBaseColorTexture;
	float hasMetallicRoughnessTexture;
	float hasNormalTexture;
	float hasOcclusionTexture;
	float hasEmissiveTexture;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
} material;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO pow(texture(albedoMap, inUV).rgb, vec3(2.2))


void main()
{		
	outColor = vec4(1.0, 0.0f, 1.0f, 1.0f);
}