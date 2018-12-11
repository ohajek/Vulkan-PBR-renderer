#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (binding = 0) uniform UBO {
	mat4 model;
	mat4 view;
	mat4 projection;
	vec3 cameraPosition;
} ubo;

layout (location = 0) out vec3 outWorldPosition;
layout (location = 1) out vec3 outNormal;

layout(push_constant) uniform PushConstants {
	vec3 objectPosition;
} pushConstants;

out gl_PerVertex  {
	vec4 gl_Position;
};


void main() 
{
	outWorldPosition = vec3(ubo.model * vec4(inPosition, 1.0f)) + pushConstants.objectPosition;
	outNormal = mat3(ubo.model) * inNormal;
	gl_Position = ubo.projection * ubo.view * vec4(outWorldPosition, 1.0);
}
