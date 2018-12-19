#pragma once

#include <VulkanRenderer.hpp>
#include <VulkanTexture.hpp>
#include <utility>
#include <VulkanBuffer.hpp>
#include "VulkanModel.hpp"



struct Vertex {
	//float pos[3];
	//float uv[2];
	glm::vec3 position;
	glm::vec2 uv;
};


class GMU : public VulkanRenderer
{	
private:
	vkpbr::Texture2D      colorMap;
	vkpbr::Texture2D      computeTarget;
	vkpbr::TextureCubemap cubeTarget;

public:

	using Vertices = struct {
		vk::PipelineVertexInputStateCreateInfo           inputState;
		std::vector<vk::VertexInputBindingDescription>   bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	};

	using GraphicsResources = struct {
		vk::Pipeline            pipeline;
		vk::PipelineLayout      pipelineLayout;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet       descriptorSetPreCompute;
		vk::DescriptorSet       descriptorSetPostCompute;
	};

	using ComputeResources = struct {
		std::vector<vk::Pipeline> pipelines;
		vk::PipelineLayout        pipelineLayout;
		uint32_t                  pipelineIndex = 0;
		uint32_t                  graphicsFamilyIndex; // family index of graphics queue -> for barriers
		vk::DescriptorSetLayout   descriptorSetLayout;
		vk::DescriptorSet         descriptorSet;
		vk::Fence                 fence; // to avoid writing to command buffer if its in use
		vk::Queue                 queue;
		vk::CommandPool           commandPool;
		vk::CommandBuffer         commandBuffer;
	};

	using UBOmatrices = struct {
		glm::mat4 model;
		glm::mat4 projection;
	};


	Vertices                 vertices;
	GraphicsResources        graphicsResources;
	ComputeResources         computeResources;
	vkpbr::Buffer            vertexBuffer;
	uint32_t                 vertexBufferSize;
	vkpbr::Buffer            indexBuffer;
	uint32_t                 indexCount;
	vkpbr::Buffer            uniformBuffer;
	UBOmatrices              uboMatrices;
	std::vector<std::string> shaderNames;


	GMU();

	~GMU() override;

	auto prepareForRender() -> void override;

	auto loadAssets() -> void;
	auto generateQuad() -> void;
	auto setupVertexDescriptions() -> void;

	auto setupDescriptorSetLayout() -> void;

	auto setupPipelines() -> void;
	auto setupDescriptorPool() -> void;

	auto setupUniformBuffers() -> void;

	auto updateUniformBuffers() -> void;
	auto setupTextureTarget(vkpbr::Texture* tex, uint32_t width, uint32_t height, vk::Format format) -> void;

	auto updateUniformParameters() -> void;

	auto setupDescriptorSets() -> void;
	auto setupComputePipeline() -> void;
	auto findComputeQueue() -> void;
	auto setupComputeCommandBuffer() -> void;

	auto setupCommandBuffers() -> void override;
	auto checkCmdBuffers() -> bool;

	auto render() -> void override;

	auto updateView() -> void override;

	auto updateUI(uint32_t object_index) -> void override;
};
