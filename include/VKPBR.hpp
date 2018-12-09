#pragma once

#include <VulkanRenderer.hpp>
#include <VulkanTexture.hpp>
#include <utility>
#include <VulkanBuffer.hpp>
#include "VulkanModel.hpp"


#define VERTEX_BUFFER_BIND_ID 0
#define GRID_DIM 7
#define OBJ_DIM 0.05f


struct Material {
	using PushBlock = struct {
		float roughness;
		float metallic;
		float r, g, b;
	};
	PushBlock parameters;
	std::string name;

	Material() {};

	Material(const std::string& name, const glm::vec3 colour, const float roughness, const float metallic) : name(name) {
		parameters.roughness = roughness;
		parameters.metallic = metallic;
		parameters.r = colour.r;
		parameters.g = colour.g;
		parameters.b = colour.b;
	};
};


class VKPBR : public VulkanRenderer
{
public:
	vkpbr::VertexLayout vertexLayout = vkpbr::VertexLayout({
		vkpbr::VertexComponent::position,
		vkpbr::VertexComponent::normal,
		vkpbr::VertexComponent::uv
	});

	using Meshes = struct {
		std::vector<vkpbr::Model> object;
		int32_t objectIndex = 1;
	};

	using UniformBuffers = struct {
		vkpbr::Buffer object;
		vkpbr::Buffer parameters;
	};

	using UBOMatrices = struct {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec3 cameraPosition;
	};

	using UBOparams = struct {
		glm::vec4 lights[4];
	};

	Meshes models;
	UniformBuffers uniformBuffers;
	UBOMatrices uboMatrices;
	UBOparams uboParams;

	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;
	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorSet descriptorSet;

	/* Default materials */
	std::vector<Material> materials;
	int32_t materialIndex = 0;
	std::vector<std::string> materialNames;
	std::vector<std::string> objectNames;

private:

public:
	VKPBR();

	~VKPBR() override;

	auto prepareForRender() -> void override;

	auto loadAssets() -> void;

	auto setupDescriptorSetLayout() -> void;

	auto setupPipelines() -> void;

	auto setupUniformBuffers() -> void;

	auto updateUniformBuffers() -> void;

	auto updateUniformParameters() -> void;

	auto setupDescriptorSets() -> void;

	auto setupCommandBuffers() -> void override;

	auto render() -> void override;

	auto updateView() -> void override;

	auto updateUI(uint32_t object_index) -> void override;
};
