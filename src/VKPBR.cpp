#include <VKPBR.hpp>

/* Tohle je magie - nesahat */
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"


VKPBR::VKPBR()
{
	windowTitle = "PGP PBR Renderer";

	camera.setPosition({ 10.0f, 13.0f, 1.8f });
	camera.setRotation({ -62.5f, 90.0f, 0.0f });
	camera.rotationSpeed = 0.25;
	camera.movementSpeed = 4.0f;
	camera.setPerspective(
		60.0f,
		static_cast<float>(settings.width) / static_cast<float>(settings.height),
		0.1f,
		256.0f
	);
	
	/* https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/ */
	materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f), 0.1f, 1.0f));
	materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f), 0.1f, 1.0f));
	materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f), 0.1f, 1.0f));
	materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f), 0.1f, 1.0f));
	materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f), 0.1f, 1.0f));
	materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f), 0.1f, 1.0f));
	materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f), 0.1f, 1.0f));
	
	/* Testing materials */
	materials.push_back(Material("White", glm::vec3(1.0f), 0.1f, 1.0f));
	materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f));
	materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.1f, 1.0f));
	materials.push_back(Material("Black", glm::vec3(0.0f), 0.1f, 1.0f));
	
	for (auto material : materials) {
		materialNames.push_back(material.name);
	}
	objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };

	materialIndex = 2;
}

VKPBR::~VKPBR()
{
	device.destroyPipeline(pipeline, nullptr);

	device.destroyPipelineLayout(pipelineLayout, nullptr);
	device.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);

	for (auto& model : models.object) {
		model.destroy();
	}

	uniformBuffers.object.destroy();
	uniformBuffers.parameters.destroy();
}

auto VKPBR::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
	loadAssets();
	setupUniformBuffers();
	setupDescriptorSetLayout();
	setupPipelines();
	setupDescriptorSets();
	setupCommandBuffers();

	preparedToRender = true;
}

auto VKPBR::loadAssets() -> void
{
	auto filenames = std::vector<std::string>{ "geosphere.obj", "teapot.dae", "venus.fbx", "torusknot.obj" };
	std::string& resource_dir = std::string(RESOURCE_DIR);
	for (const auto& file : filenames) {
		vkpbr::Model model;
		auto path = std::string(resource_dir + "models/" + file);
		model.loadFromFile(path, vertexLayout, OBJ_DIM * (file == "venus.fbx" ? 3.0f : 1.0f), vulkanDevice.get(), queue);
		models.object.push_back(model);
	}	
}

auto VKPBR::setupDescriptorSetLayout() -> void
{
	// TODO: mozna zmenit inicializaci
	auto setLayoutBindings = std::vector<vk::DescriptorSetLayoutBinding>{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
		//(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0),
		//(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1)
	};


	vk::DescriptorSetLayoutCreateInfo descriptor_layout = {};
	descriptor_layout.pBindings = setLayoutBindings.data();
	descriptor_layout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

	VK_ASSERT(device.createDescriptorSetLayout(&descriptor_layout, nullptr, &descriptorSetLayout));

	vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.pSetLayouts = &descriptorSetLayout;
	pipeline_layout_create_info.setLayoutCount = 1;

	auto push_constant_ranges = std::vector<vk::PushConstantRange>{
		vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::vec3)),
		vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, sizeof(glm::vec3), sizeof(Material::PushBlock))
	};

	pipeline_layout_create_info.pushConstantRangeCount = 2;
	pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_create_info, nullptr, &pipelineLayout));
}

auto VKPBR::setupDescriptorSets() -> void
{
	/* First descriptor pool */
	// TODO: mozna predelat init
	auto poolSizes = std::vector<vk::DescriptorPoolSize>{
		//(vk::DescriptorType::eUniformBuffer, 4),
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 4),
	};

	vk::DescriptorPoolCreateInfo pool_create_info = {};
	pool_create_info.pPoolSizes = poolSizes.data();
	pool_create_info.poolSizeCount = poolSizes.size();
	pool_create_info.maxSets = 2;

	VK_ASSERT(device.createDescriptorPool(&pool_create_info, nullptr, &descriptorPool));

	/* Descriptor sets */
	vk::DescriptorSetAllocateInfo allocate_info = {};
	allocate_info.descriptorPool = descriptorPool;
	allocate_info.pSetLayouts = &descriptorSetLayout;
	allocate_info.descriptorSetCount = 1;
	
	/* 3D object desc. set */
	VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &descriptorSet));

	vk::WriteDescriptorSet object_set = {};
	object_set.dstSet = descriptorSet;
	object_set.descriptorType = vk::DescriptorType::eUniformBuffer;
	object_set.dstBinding = 0;
	object_set.pBufferInfo = &uniformBuffers.object.descriptor;
	object_set.descriptorCount = 1;

	vk::WriteDescriptorSet parameters_set = {};
	parameters_set.dstSet = descriptorSet;
	parameters_set.descriptorType = vk::DescriptorType::eUniformBuffer;
	parameters_set.dstBinding = 1;
	parameters_set.pBufferInfo = &uniformBuffers.parameters.descriptor;
	parameters_set.descriptorCount = 1;

	auto write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{
		object_set,
		parameters_set
	};

	device.updateDescriptorSets(static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
}

auto VKPBR::setupPipelines() -> void
{
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state = {};
	input_assembly_state.topology = vk::PrimitiveTopology::eTriangleList;
	input_assembly_state.flags = static_cast<vk::PipelineInputAssemblyStateCreateFlagBits>(0);
	input_assembly_state.primitiveRestartEnable = false;

	vk::PipelineRasterizationStateCreateInfo rasterization_state = {};
	rasterization_state.polygonMode = vk::PolygonMode::eFill;
	rasterization_state.cullMode = vk::CullModeFlagBits::eBack;
	rasterization_state.frontFace = vk::FrontFace::eCounterClockwise;
	rasterization_state.flags = static_cast<vk::PipelineRasterizationStateCreateFlagBits>(0);
	rasterization_state.lineWidth = 1.0f;

	vk::PipelineColorBlendAttachmentState blend_attachment = {};
	blend_attachment.colorWriteMask = static_cast<vk::ColorComponentFlags>(0xf);
	blend_attachment.blendEnable = false;

	vk::PipelineColorBlendStateCreateInfo color_blend_state = {};
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &blend_attachment;

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state = {};
	depth_stencil_state.depthTestEnable = false;
	depth_stencil_state.depthWriteEnable = false;
	depth_stencil_state.depthCompareOp = vk::CompareOp::eLessOrEqual;

	vk::PipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	//TODO: flas = 0;

	vk::PipelineMultisampleStateCreateInfo multisample_state = {};
	multisample_state.rasterizationSamples = vk::SampleCountFlagBits::e1;
	//todo: flags= 0;

	auto dynamic_states = std::vector<vk::DynamicState> {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo dynamic_state;
	dynamic_state.pDynamicStates = dynamic_states.data();
	dynamic_state.dynamicStateCount = dynamic_states.size();
	//todo :: flags

	vk::GraphicsPipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.layout = pipelineLayout;
	pipeline_create_info.renderPass = renderPass;
	pipeline_create_info.basePipelineHandle = nullptr;
	pipeline_create_info.basePipelineIndex = -1;

	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;

	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState = &color_blend_state;
	pipeline_create_info.pMultisampleState = &multisample_state;
	pipeline_create_info.pViewportState = &viewport_state;
	pipeline_create_info.pDepthStencilState = &depth_stencil_state;
	pipeline_create_info.pDynamicState = &dynamic_state;
	pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	pipeline_create_info.pStages = shader_stages.data();

	/* Vertexx bindings and attributes */
	/* Binding description */
	auto vertex_input_bindings = std::vector<vk::VertexInputBindingDescription>{
		vk::VertexInputBindingDescription(0, vertexLayout.stride(), vk::VertexInputRate::eVertex),
	};

	/* Attribute description */
	auto vertex_input_Attributes = std::vector<vk::VertexInputAttributeDescription>{
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0), // Position
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3), // Normal
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_state = {};
	vertex_input_state.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_Attributes.size());
	vertex_input_state.pVertexAttributeDescriptions = vertex_input_Attributes.data();

	pipeline_create_info.pVertexInputState = &vertex_input_state;

	/* PBR pipeline */
	shader_stages[0] = loadShaderFromFile(device, "pbr_basic.vert.spv", vk::ShaderStageFlagBits::eVertex);
	shader_stages[1] = loadShaderFromFile(device, "pbr_basic.frag.spv", vk::ShaderStageFlagBits::eFragment);

	/* Enable depth test and write */
	depth_stencil_state.depthTestEnable = true;
	depth_stencil_state.depthWriteEnable = true;

	/* Flip cull mode */
	rasterization_state.cullMode = vk::CullModeFlagBits::eFront;

	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &pipeline_create_info, nullptr, &pipeline));
}

auto VKPBR::setupUniformBuffers() -> void
{
	/* Vertex shader uniform buffer */
	vulkanDevice->createBuffer(
		sizeof(uboMatrices),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffers.object
	);

	vulkanDevice->createBuffer(
		sizeof(uboParams),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffers.parameters
	);

	VK_ASSERT(uniformBuffers.object.map());
	VK_ASSERT(uniformBuffers.parameters.map());

	updateUniformBuffers();
	updateUniformParameters();
}

auto VKPBR::updateUniformBuffers() -> void
{
	/* 3D object parameters */
	uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
	uboMatrices.view = camera.matrices.view;
	uboMatrices.projection = camera.matrices.perspective;
	uboMatrices.cameraPosition = camera.position * -1.0f;

	memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
}

auto VKPBR::updateUniformParameters() -> void
{
	const auto p = 15.0f;
	uboParams.lights[0] = glm::vec4(-p, -p * 0.5f, -p, 1.0f);
	uboParams.lights[1] = glm::vec4(-p, -p * 0.5f, p, 1.0f);
	uboParams.lights[2] = glm::vec4(p, -p * 0.5f, p, 1.0f);
	uboParams.lights[3] = glm::vec4(p, -p * 0.5f, -p, 1.0f);

	memcpy(uniformBuffers.parameters.mapped, &uboParams, sizeof(uboParams));
}

auto VKPBR::setupCommandBuffers() -> void
{
	vk::CommandBufferBeginInfo buffer_begin_info = {};

	const auto clear_values = std::vector<vk::ClearValue>{
		vk::ClearColorValue(
			std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f}
		),
		vk::ClearDepthStencilValue(1.0f, 0)
	};

	vk::RenderPassBeginInfo renderpass_info = {};
	renderpass_info.renderPass = renderPass;
	renderpass_info.renderArea.offset.x = 0;
	renderpass_info.renderArea.offset.y = 0;
	renderpass_info.renderArea.extent.width = settings.width;
	renderpass_info.renderArea.extent.height = settings.height;
	renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	renderpass_info.pClearValues = clear_values.data();

	for (auto i = 0; i < drawCalls.size(); ++i) {
		/* Target frame buffer*/
		renderpass_info.framebuffer = framebuffers[i];

		VK_ASSERT(drawCalls[i].begin(&buffer_begin_info));
		drawCalls[i].beginRenderPass(&renderpass_info, vk::SubpassContents::eInline);

		vk::Viewport viewport = {};
		viewport.width = static_cast<float>(settings.width);
		viewport.height = static_cast<float>(settings.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		drawCalls[i].setViewport(0, 1, &viewport);

		vk::Rect2D scissor = {};
		scissor.extent.width = settings.width;
		scissor.extent.height = settings.height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		drawCalls[i].setScissor(0, 1, &scissor);

		vk::DeviceSize offsets[1] = { 0 };

		/* Objects */
		drawCalls[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		drawCalls[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		drawCalls[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1, &models.object[models.objectIndex].vertices.buffer, offsets);
		drawCalls[i].bindIndexBuffer(models.object[models.objectIndex].indices.buffer, 0, vk::IndexType::eUint32);

		auto material = materials[materialIndex];

		/* Single row */
		/*
		material.parameters.metallic = 1.0f;

		const uint32_t object_count = 10;
		for (uint32_t j = 0; j < object_count; j++) {
			glm::vec3 position = glm::vec3(float(j - (object_count / 2.0f)) * 2.5f, 0.0f, 0.0f);
			material.parameters.roughness = glm::clamp(static_cast<float>(j) / static_cast<float>(object_count), 0.005f, 1.0f);

			drawCalls[i].pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::vec3), &position);
			drawCalls[i].pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::vec3), sizeof(Material::PushBlock), &material);
			drawCalls[i].drawIndexed(models.object[models.objectIndex].indexCount, 1, 0, 0, 0);
		}
		*/

		/* Grid */
		for (uint32_t y = 0; y < GRID_DIM; y++) {
			for (uint32_t x = 0; x < GRID_DIM; x++) {
				glm::vec3 pos = glm::vec3(float(x - (GRID_DIM / 2.0f)) * 2.5f, 0.0f, float(y - (GRID_DIM / 2.0f)) * 2.5f);
				drawCalls[i].pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::vec3), &pos);
				material.parameters.metallic = glm::clamp((float)x / (float)(GRID_DIM - 1), 0.1f, 1.0f);
				material.parameters.roughness = glm::clamp((float)y / (float)(GRID_DIM - 1), 0.05f, 1.0f);
				drawCalls[i].pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::vec3), sizeof(Material::PushBlock), &material);
				drawCalls[i].drawIndexed(models.object[models.objectIndex].indexCount, 1, 0, 0, 0);
			}
		}

		// draw ui

		drawCalls[i].endRenderPass();
		drawCalls[i].end();
	}	
}

auto VKPBR::render() -> void
{
	if (!preparedToRender) {
		return;
	}

	VulkanRenderer::prepareFrame();

	VK_ASSERT(device.waitForFences(1, &memoryFences[currentBuffer], true, UINT64_MAX));
	VK_ASSERT(device.resetFences(1, &memoryFences[currentBuffer]));

	const vk::PipelineStageFlags wait_dst_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::SubmitInfo submit_info = {};
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &presentCompleteSemaphore;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &renderCompleteSemaphore;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &drawCalls[currentBuffer];

	VK_ASSERT(queue.submit(1, &submit_info, memoryFences[currentBuffer]));

	VulkanRenderer::submitFrame();
}

auto VKPBR::updateView() -> void
{
	updateUniformBuffers();
}

auto VKPBR::updateUI(const uint32_t object_index) -> void
{
	models.objectIndex = object_index;
	updateUniformBuffers();
	setupCommandBuffers();
}
