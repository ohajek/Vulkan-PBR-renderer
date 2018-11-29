#include <VKPBR.hpp>

/* Tohle je magie - nesahat */
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"


VKPBR::VKPBR()
{
	windowTitle = "vkPBR renderer";

	camera.setPerspective(
		45.0f,
		static_cast<float>(settings.width) / static_cast<float>(settings.height),
		0.1f,
		256.0f
	);
	
	camera.rotationSpeed = 0.25;
	camera.movementSpeed = 0.1f;

	camera.setPosition({ 0.0f, 0.0f, 2.5f });
	camera.setRotation({ 0.0f, 0.0f, 0.0f });
}

VKPBR::~VKPBR()
{
	/*
	//device.destroyPipeline(pipelines.skybox, nullptr);
	device.destroyPipeline(pipelines.pbr, nullptr);
	device.destroyPipeline(pipelines.pbrAlphaBlend, nullptr);

	device.destroyPipelineLayout(pipelineLayout, nullptr);

	device.destroyDescriptorSetLayout(descriptorSetLayouts.scene, nullptr);
	device.destroyDescriptorSetLayout(descriptorSetLayouts.node, nullptr);
	device.destroyDescriptorSetLayout(descriptorSetLayouts.material, nullptr);

	models.scene.release(device);
	//models.skybox.release(device);

	device.destroyBuffer(uniformBuffers.scene.buffer, nullptr);
	//device.destroyBuffer(uniformBuffers.skybox.buffer, nullptr);
	device.destroyBuffer(uniformBuffers.parameters.buffer, nullptr);

	device.freeMemory(uniformBuffers.scene.memory, nullptr);
	//device.freeMemory(uniformBuffers.skybox.memory, nullptr);
	device.freeMemory(uniformBuffers.parameters.memory, nullptr);

	textures.empty.release();
	//textures.lutBRDF.release();
	//*/
}

auto VKPBR::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
	loadAssets();
	setupPipelines();

	preparedToRender = true;
}

auto VKPBR::loadAssets() -> void
{
	const auto& resource_path = std::string(RESOURCE_DIR);
	const auto& test_scene_file = resource_path + "models/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf";

	textures.empty.loadFromFile(resource_path + "textures/empty.ktx", vk::Format::eR8G8B8A8Unorm, vulkanDevice.get(), queue);
	models.scene.loadFromFile(test_scene_file, vulkanDevice.get(), queue);

	uboMatrices.flipUV = 1.0f;
	scale = 1.0f / models.scene.dimensions.radius;
	camera.setPosition(glm::vec3(-models.scene.dimensions.center.x * scale, -models.scene.dimensions.center.y * scale, camera.position.z));
}

auto VKPBR::setupPipelines() -> void
{
	/* Fixed state */
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
	input_assembly_state_create_info.topology = vk::PrimitiveTopology::eTriangleList;
	input_assembly_state_create_info.primitiveRestartEnable = false;

	vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
	rasterization_state_create_info.polygonMode = vk::PolygonMode::eFill;
	rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
	rasterization_state_create_info.frontFace = vk::FrontFace::eCounterClockwise;
	rasterization_state_create_info.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisample_state_create_info = {};

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
	depth_stencil_state_create_info.depthWriteEnable = false;
	depth_stencil_state_create_info.depthTestEnable = false;
	depth_stencil_state_create_info.depthCompareOp = vk::CompareOp::eLessOrEqual;
	depth_stencil_state_create_info.front = depth_stencil_state_create_info.back;
	depth_stencil_state_create_info.back.compareOp = vk::CompareOp::eAlways;

	vk::PipelineColorBlendAttachmentState blend_attachment_state = {};
	blend_attachment_state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blend_attachment_state.blendEnable = false;

	vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
	color_blend_state_create_info.attachmentCount = 1;
	color_blend_state_create_info.pAttachments = &blend_attachment_state;

	const auto dynamic_states = std::array<vk::DynamicState, 2> { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamic_state_create_info = {};
	dynamic_state_create_info.dynamicStateCount = dynamic_states.size();
	dynamic_state_create_info.pDynamicStates = dynamic_states.data();

	vk::PipelineViewportStateCreateInfo viewport_state_create_info = {};
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.scissorCount = 1;


	/* Pipeline layout */
	vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.setLayoutCount = 0;
	pipeline_layout_create_info.pSetLayouts = nullptr;
	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_create_info, nullptr, &pipelineLayout));

	/* Vertex binding */
	vk::VertexInputBindingDescription vertex_input_binding = { 0, sizeof(vkpbr::gltf::Model::Vertex), vk::VertexInputRate::eVertex };
	auto vertex_input_attributes = std::vector<vk::VertexInputAttributeDescription>{
		{ 0, 0, vk::Format::eR32G32B32Sfloat, 0 }, // position
		{ 1, 0, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3 }, // normal
		{ 2, 0, vk::Format::eR32G32Sfloat, sizeof(float) * 6 } // UV
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
	vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding;
	vertex_input_state_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attributes.data();

	/* Setting up all pipelines */
	auto shader_stages = std::array<vk::PipelineShaderStageCreateInfo, 2>{};

	vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
	graphics_pipeline_create_info.layout = pipelineLayout;
	graphics_pipeline_create_info.renderPass = renderPass;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
	graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
	graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisample_state_create_info;
	graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
	graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
	graphics_pipeline_create_info.pDynamicState = &dynamic_state_create_info;
	graphics_pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	graphics_pipeline_create_info.pStages = shader_stages.data();

	/* PBR pipeline*/
	shader_stages = {
		loadShaderFromFile(device, "pbr_shader.vert.spv", vk::ShaderStageFlagBits::eVertex),
		loadShaderFromFile(device, "pbr_shader.frag.spv", vk::ShaderStageFlagBits::eFragment)
	};

	depth_stencil_state_create_info.depthWriteEnable = true;
	depth_stencil_state_create_info.depthTestEnable = true;
	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &graphics_pipeline_create_info, nullptr, &pipelines.pbr));

	for (auto& shader_stage : shader_stages) {
		device.destroyShaderModule(shader_stage.module, nullptr);
	}
}

auto VKPBR::render() -> void
{
	if (!preparedToRender) {
		return;
	}
}