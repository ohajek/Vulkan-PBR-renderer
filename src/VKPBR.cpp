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

}

auto VKPBR::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
	loadAssets();
	setupUniformBuffers();
	setupDescriptors();
	setupPipelines();
	setupCommandBuffers();

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

	/* Dynamic state */
	const auto dynamic_states = std::array<vk::DynamicState, 2> { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamic_state_create_info = {};
	dynamic_state_create_info.dynamicStateCount = dynamic_states.size();
	dynamic_state_create_info.pDynamicStates = dynamic_states.data();

	vk::PipelineViewportStateCreateInfo viewport_state_create_info = {};
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.scissorCount = 1;

	/* Pipeline layout */
	const auto set_layouts = std::vector<vk::DescriptorSetLayout> {
		descriptorSetLayouts.scene
	};

	vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.setLayoutCount = set_layouts.size();
	pipeline_layout_create_info.pSetLayouts = set_layouts.data();
	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_create_info, nullptr, &pipelineLayout));

	/* Vertex binding */
	vk::VertexInputBindingDescription vertex_input_binding = { 0, sizeof(vkpbr::gltf::Model::Vertex), vk::VertexInputRate::eVertex };
	auto vertex_input_attributes = std::vector<vk::VertexInputAttributeDescription>{
		{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vkpbr::gltf::Model::Vertex, position) }, // position
		{ 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vkpbr::gltf::Model::Vertex, normal) }, // normals
		{ 2, 0, vk::Format::eR32G32Sfloat, offsetof(vkpbr::gltf::Model::Vertex, uv) }, // UV
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
		loadShaderFromFile(device, "triangle.vert.spv", vk::ShaderStageFlagBits::eVertex),
		loadShaderFromFile(device, "triangle.frag.spv", vk::ShaderStageFlagBits::eFragment)
	};

	depth_stencil_state_create_info.depthWriteEnable = true;
	depth_stencil_state_create_info.depthTestEnable = true;
	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &graphics_pipeline_create_info, nullptr, &pipelines.pbr));

	/* Clean up */
	for (auto& shader_stage : shader_stages) {
		device.destroyShaderModule(shader_stage.module, nullptr);
	}
}

auto VKPBR::setupUniformBuffers() -> void
{
	/* Vertex shader scene uniform buffer */
	VK_ASSERT(vulkanDevice->createBuffer(
		sizeof(uboMatrices),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffers.scene.buffer,
		uniformBuffers.scene.memory
	));


	/* Descriptors */
	uniformBuffers.scene.descriptor.buffer = uniformBuffers.scene.buffer;
	uniformBuffers.scene.descriptor.offset = 0;
	uniformBuffers.scene.descriptor.range = sizeof(uboMatrices);

	
	/* Persistent memory mapping */
	VK_ASSERT(device.mapMemory(uniformBuffers.scene.memory, 0, sizeof(uboMatrices), static_cast<vk::MemoryMapFlagBits>(0), &uniformBuffers.scene.mappedMemory));

	updateUniformBuffers();
}

auto VKPBR::updateUniformBuffers() -> void
{
	// Scene
	uboMatrices.projection = camera.matrices.perspective;
	uboMatrices.view = camera.matrices.view;

	uboMatrices.model = glm::translate(glm::mat4(1.0f), modelPosition);
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	uboMatrices.model = glm::scale(uboMatrices.model, glm::vec3(scale));

	uboMatrices.cameraPosition = glm::vec3(
		-camera.position.z * sin(glm::radians(camera.rotation.y)) * cos(glm::radians(camera.rotation.x)),
		-camera.position.z * sin(glm::radians(camera.rotation.x)),
		camera.position.z * cos(glm::radians(camera.rotation.y)) * cos(glm::radians(camera.rotation.x))
	);

	memcpy(uniformBuffers.scene.mappedMemory, &uboMatrices, sizeof(uboMatrices));
}

auto VKPBR::updateUniformParameters() -> void
{
	uboParameters.lightDirection = glm::vec4(
		sin(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
		sin(glm::radians(lightSource.rotation.y)),
		cos(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
		0.0f);
	memcpy(uniformBuffers.parameters.mappedMemory, &uboParameters, sizeof(uboParameters));
}

auto VKPBR::setupDescriptors() -> void
{
	auto pool_sizes = std::vector<vk::DescriptorPoolSize> {
		{ vk::DescriptorType::eUniformBuffer, 1 },
	};
	vk::DescriptorPoolCreateInfo descriptor_pool_create_info = {};
	descriptor_pool_create_info.poolSizeCount = 1;
	descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
	descriptor_pool_create_info.maxSets = static_cast<uint32_t>(swapchain.images.size()); //possibly +2
	VK_ASSERT(device.createDescriptorPool(&descriptor_pool_create_info, nullptr, &descriptorPool));

	// Scene (matrices)
	{
		auto set_layout_bindings = std::vector<vk::DescriptorSetLayoutBinding> {
			{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr }
		};
		vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
		descriptor_set_layout_create_info.pBindings = set_layout_bindings.data();
		descriptor_set_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
		VK_ASSERT(device.createDescriptorSetLayout(&descriptor_set_layout_create_info, nullptr, &descriptorSetLayouts.scene));

		vk::DescriptorSetAllocateInfo descriptor_set_allocate_info = {};
		descriptor_set_allocate_info.descriptorPool = descriptorPool;
		descriptor_set_allocate_info.pSetLayouts = &descriptorSetLayouts.scene;
		descriptor_set_allocate_info.descriptorSetCount = 1;
		VK_ASSERT(device.allocateDescriptorSets(&descriptor_set_allocate_info, &descriptorSets.scene));

		auto write_descriptor_sets = std::array<vk::WriteDescriptorSet, 1> {};

		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		write_descriptor_sets[0].dstSet = descriptorSets.scene;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].pBufferInfo = &uniformBuffers.scene.descriptor;

		device.updateDescriptorSets(static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
	}
}

auto VKPBR::setupCommandBuffers() -> void
{
	vk::CommandBufferBeginInfo begin_info = {};

	const auto clear_values = std::vector<vk::ClearValue> {
		vk::ClearColorValue(
			std::array<float,4>{0.2f, 0.3f, 0.3f, 1.0f}
		),
		vk::ClearDepthStencilValue(1.0f, 0)
	};

	vk::RenderPassBeginInfo renderpass_begin_info = {};
	renderpass_begin_info.renderPass = renderPass;
	renderpass_begin_info.renderArea.offset.x = 0;
	renderpass_begin_info.renderArea.offset.y = 0;
	renderpass_begin_info.renderArea.extent.width = settings.width;
	renderpass_begin_info.renderArea.extent.height = settings.height;
	renderpass_begin_info.clearValueCount = clear_values.size();
	renderpass_begin_info.pClearValues = clear_values.data();

	for (size_t i = 0; i < drawCalls.size(); ++i) {
		renderpass_begin_info.framebuffer = framebuffers[i];

		VK_ASSERT(drawCalls[i].begin(&begin_info));
		drawCalls[i].beginRenderPass(&renderpass_begin_info, vk::SubpassContents::eInline);

		vk::Viewport viewport = {};
		viewport.width = static_cast<float>(settings.width);
		viewport.height = static_cast<float>(settings.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		drawCalls[i].setViewport(0, 1, &viewport);

		vk::Rect2D scissors = {};
		scissors.extent = vk::Extent2D{ settings.width, settings.height };
		drawCalls[i].setScissor(0, 1, &scissors);

		vk::DeviceSize offsets[1] = { 0 };

		drawCalls[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.pbr);

		auto& model = models.scene;

		drawCalls[i].bindVertexBuffers(0, 1, &model.vertices.buffer, offsets);
		drawCalls[i].bindIndexBuffer(model.indices.buffer, 0, vk::IndexType::eUint32);
		
		/*
		for (auto node : model.nodes) {
			renderModelNode(node, drawCalls[i], vkpbr::gltf::Material::AlphaMode::opaque);
		}
		*/

		drawCalls[i].endRenderPass();
		drawCalls[i].end();
	}
}

auto VKPBR::renderModelNode(vkpbr::gltf::Node* node, vk::CommandBuffer cmd_buffer, const vkpbr::gltf::Material::AlphaMode alpha_mode) const -> void
{
	if (node->mesh) {
		for (auto* primitive : node->mesh->primitives) {
			if (primitive->material.alphaMode == alpha_mode) {

				const auto descriptor_sets = std::vector<vk::DescriptorSet>{
					descriptorSets.scene,
				};

				cmd_buffer.bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics,
					pipelineLayout,
					0,
					static_cast<uint32_t>(descriptor_sets.size()),
					descriptor_sets.data(),
					0,
					nullptr
				);

				cmd_buffer.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
	}

	for (auto& child : node->children) {
		renderModelNode(child, cmd_buffer, alpha_mode);
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