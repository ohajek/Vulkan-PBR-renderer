#version 450

layout (location = 0) in vec3 inWorldPosition;
layout (location = 1) in vec3 inNormal;

layout (binding = 0) uniform UBO {
	mat4 model;
	mat4 view;
	mat4 projection;
	vec3 cameraPosition;
} ubo;

layout (binding = 1) uniform UBOparameters {
	vec4 lights[4];
} uboParameters;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
	layout(offset = 12) float roughness;
	layout(offset = 16) float metallic;
	layout(offset = 20) float r;
	layout(offset = 24) float g;
	layout(offset = 28) float b;
} material;

const float pi = 3.14159265359;


// Returns albedo colour of location
vec3 getAlbedo()
{
	return vec3(material.r, material.g, material.b);
}

// D component
float chiGGX(float v)
{
    return v > 0 ? 1 : 0;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float NdotH = dot(N,H);
	float NdotH2 = NdotH * NdotH;
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denominator = NdotH2 * alpha2 + (1 - NdotH2);
	   
	return (chiGGX(NdotH) * alpha2) / ( pi * denominator * denominator );
}


// F component 
vec3 fresnelSchlick(float cosTheta, float metallic)
{
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, getAlbedo(), metallic);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


// G component
float GeometrySchlickGGX(float NoV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float numerator   = NoV;
    float denominator = NoV * (1.0 - k) + k;
	
    return numerator / denominator;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}



vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
{
	vec3 H = normalize(V + L);
	
	vec3 lightColour = vec3(1.0f);
	vec3 colour = vec3(0.0f);
  
	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);

	if (NdotL > 0.0) {
		// Cook-Torrance BRDF
		float D = DistributionGGX(N, H, material.roughness);       
		float G   = GeometrySmith(N, V, L, material.roughness);
		vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), metallic);       

		vec3 specular = (D * F * G) / (4.0f * NdotL * NdotV);    

		colour += specular * NdotL * lightColour;
	}

	return colour;
}


void main()
{		  
	vec3 N = normalize(inNormal);
	vec3 V = normalize(ubo.cameraPosition - inWorldPosition);
	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, getAlbedo(), material.metallic);

	// Solve reflectance function
	vec3 Lo = vec3(0.0f);
	for (int i = 0; i < uboParameters.lights.length(); i++) {
		vec3 L = normalize(uboParameters.lights[i].xyz - inWorldPosition);
		Lo += BRDF(L, V, N, material.metallic, material.roughness);
	}

	vec3 color = vec3(0.03) * getAlbedo();
	color += Lo;  

	// Gamma correction
	color = color / (color + vec3(1.0f));
	color = pow(color, vec3(1.0f / 2.2f)); 

	outColor = vec4(color, 1.0f);
}