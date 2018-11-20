#pragma once

#include <VulkanRenderer.hpp>
#include <VulkanTexture.hpp>
#include <utility>
#include <gltfModel.hpp>
#include <Camera.hpp>


class VKPBR : public VulkanRenderer
{
public:
	VKPBR();

	~VKPBR() override;

	auto prepareForRender() -> void override;
	auto setupPipelines() -> void;
	auto setupDescriptors() -> void;
	auto setupUniformBuffers() -> void;
	auto updateUniformBuffers() -> void;
	auto updateParameters() -> void;

	auto render() -> void override;
	auto renderGLTFNode(vkpbr::gltf::Node* node, vk::CommandBuffer, vkpbr::gltf::Material::AlphaMode alpha_mode) const -> void;
	auto createCommandBuffers() -> void;
	auto loadAssets() -> void;
	auto setupNodeDescriptorSet(vkpbr::gltf::Node* node) const -> void;

	using Textures = struct {
		Texture2D empty;
		Texture2D lutBRDF;
	};

	using Models = struct {
		gltf::Model scene;
		gltf::Model skybox;
	};

	using Buffer = struct {
		vk::Buffer               buffer;
		vk::DeviceMemory         memory;
		vk::DescriptorBufferInfo descriptor;
		void*                    mappedMemory;
	};

	using UniformBuffers = struct {
		Buffer scene;
		Buffer skybox;
		Buffer parameters;
	};

	using UBOMatrices = struct {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec3 cameraPosition;
		float     flipUV = 0.0f;
	};

	using UBOParameters = struct {
		glm::vec4 lightDirection = {};
		float     exposure = 4.5f;
		float     gamma = 2.2f;
		float     prefilteredMipLevels;
	};

	using Pipelines = struct {
		vk::Pipeline skybox;
		vk::Pipeline pbr;
		vk::Pipeline pbrAlphaBlend;
	};

	using DescriptorSetLayouts = struct {
		vk::DescriptorSetLayout scene;
		vk::DescriptorSetLayout material;
		vk::DescriptorSetLayout node;
	};

	using DescriptorSets = struct {
		vk::DescriptorSet scene;
		vk::DescriptorSet skybox;
	};

	using LightSource = struct {
		glm::vec3 color = glm::vec3(1.0f);
		glm::vec3 rotation = glm::vec3(75.0f, 40.0f, 0.0f);
	};

	using PushConstantBlockMaterial = struct {
		glm::vec4 baseColorFactor;
		glm::vec4 emissiveFactor;
		glm::vec4 diffuseFactor;
		glm::vec4 specularFactor;

		float workflow;
		float hasColorTexture;
		float hasPhysicalDescriptorTexture;
		float hasNormalTexture;
		float hasOcclusionTexture;
		float hasEmissiveTexture;
		float metallicFactor;
		float roughnessFactor;
		float alphaMask;
		float alphaMaskCutoff;
	};

	Textures                  textures;
	Models                    models;
	UniformBuffers            uniformBuffers;
	UBOMatrices               uboMatrices;
	UBOParameters             uboParameters;
	Pipelines                 pipelines;
	DescriptorSetLayouts      descriptorSetLayouts;
	DescriptorSets            descriptorSets;
	LightSource               lightSource;
	PushConstantBlockMaterial pushConstantBlockMaterial;
	vk::PipelineLayout        pipelineLayout;
	float                     scale = 1.0f;
	Camera                    camera;

	bool      rotateModel = false;
	glm::vec3 modelRotation = glm::vec3(0.0f);
	glm::vec3 modelPosition = glm::vec3(0.0f);

	enum class PBRworkflow {
		metallic_roughness = 0,
		specular_glosiness = 1
	};
};
