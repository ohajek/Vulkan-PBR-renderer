#include <VKPBR.hpp>

/* Tohle je magie - nesahat */
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"


GMU::GMU()
{
	windowTitle = "GMU PBR compute";
}

GMU::~GMU()
{

}

auto GMU::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
	loadAssets();
	generateQuad();
	setupVertexDescriptions();
	setupUniformBuffers();
	setupTextureTarget(&computeTarget, colorMap.width, colorMap.height, vk::Format::eR8G8B8A8Unorm);
	setupDescriptorSetLayout();
	setupPipelines();
	setupDescriptorPool();
	setupDescriptorSets();
	setupComputePipeline();
	setupCommandBuffers();

	preparedToRender = true;
}

auto GMU::loadAssets() -> void
{
	const auto resource_path = std::string(RESOURCE_DIR);
	colorMap.loadFromFile(
		resource_path + "textures/vulkan.ktx",
		vk::Format::eR8G8B8A8Unorm,
		vulkanDevice.get(),
		queue, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
		vk::ImageLayout::eGeneral
	);
}

auto GMU::generateQuad() -> void
{
	auto  vertices = std::vector<Vertex> {
		Vertex { glm::vec3(-1.0f, 1.0f, 0.0f),	glm::vec2(0.0f, 1.0f) },
		Vertex { glm::vec3(1.0f, 1.0f, 0.0f),	glm::vec2(1.0f, 1.0f) },
		Vertex { glm::vec3(1.0f, -1.0f, 0.0f),	glm::vec2(1.0f, 0.0f) },
		Vertex { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
	};

	auto indices = std::vector<uint32_t>{ 0, 1, 2,  2, 3, 0 };
	indexCount = indices.size();

	/* Vertex buffer */
	vulkanDevice->createBuffer(
		vertices.size() * sizeof(Vertex),
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		vertexBuffer,
		vertices.data()
	);

	/* Index buffer */
	vulkanDevice->createBuffer(
		indices.size() * sizeof(uint32_t),
		vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		indexBuffer,
		indices.data()
	);
}

auto GMU::setupVertexDescriptions() -> void
{
	/* Vertex binding */
	vk::VertexInputBindingDescription binding_description = {};
	binding_description.binding = 0;
	binding_description.stride = sizeof(Vertex);
	binding_description.inputRate = vk::VertexInputRate::eVertex;

	vertices.bindingDescriptions.push_back(binding_description);

	/* Attributes */
	vk::VertexInputAttributeDescription position_attribute = {};
	position_attribute.binding = 0;
	position_attribute.location = 0;
	position_attribute.format = vk::Format::eR32G32B32Sfloat;
	position_attribute.offset = offsetof(Vertex, position);

	vk::VertexInputAttributeDescription uv_attribute = {};
	uv_attribute.binding = 0;
	uv_attribute.location = 1;
	uv_attribute.format = vk::Format::eR32G32Sfloat;
	uv_attribute.offset = offsetof(Vertex, uv);

	vertices.attributeDescriptions.push_back(position_attribute);
	vertices.attributeDescriptions.push_back(uv_attribute);

	/* Assign to vertex buffer */
	vk::PipelineVertexInputStateCreateInfo input_state = {};
	input_state.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
	input_state.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	input_state.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
	input_state.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	vertices.inputState = input_state;
}

auto GMU::setupDescriptorSetLayout() -> void
{
	auto setLayoutBindings = std::vector<vk::DescriptorSetLayoutBinding>{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
	};

	auto descriptor_layout = vk::DescriptorSetLayoutCreateInfo{};
	descriptor_layout.pBindings = setLayoutBindings.data();
	descriptor_layout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	VK_ASSERT(device.createDescriptorSetLayout(&descriptor_layout, nullptr, &graphicsResources.descriptorSetLayout));

	auto pipeline_layout_info = vk::PipelineLayoutCreateInfo{};
	pipeline_layout_info.pSetLayouts = &graphicsResources.descriptorSetLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_info, nullptr, &graphicsResources.pipelineLayout));
}

auto GMU::setupDescriptorSets() -> void
{
	vk::DescriptorSetAllocateInfo allocate_info = {};
	allocate_info.descriptorPool = descriptorPool;
	allocate_info.pSetLayouts = &graphicsResources.descriptorSetLayout;
	allocate_info.descriptorSetCount = 1;

	/* Input image before compute */
	VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &graphicsResources.descriptorSetPreCompute));

	auto base_image_write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{
		createWriteDescriptorSet(graphicsResources.descriptorSetPreCompute, vk::DescriptorType::eUniformBuffer, 0, &uniformBuffer.descriptor),
		createWriteDescriptorSet(graphicsResources.descriptorSetPreCompute, vk::DescriptorType::eCombinedImageSampler, 1, &colorMap.descriptorInfo),
	};
	device.updateDescriptorSets(base_image_write_descriptor_sets.size(), base_image_write_descriptor_sets.data(), 0, nullptr);

	/* Image after compute shader processing */
	VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &graphicsResources.descriptorSetPostCompute));
	auto write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{
		createWriteDescriptorSet(graphicsResources.descriptorSetPostCompute, vk::DescriptorType::eUniformBuffer, 0, &uniformBuffer.descriptor),
		createWriteDescriptorSet(graphicsResources.descriptorSetPostCompute, vk::DescriptorType::eCombinedImageSampler, 1, &computeTarget.descriptorInfo),
	};
	device.updateDescriptorSets(write_descriptor_sets.size(), write_descriptor_sets.data(), 0, nullptr);
}

auto GMU::setupComputePipeline() -> void
{
	findComputeQueue();


	auto set_layout_bindings = std::vector<vk::DescriptorSetLayoutBinding>{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute), // Binding 0: input image RO
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute), // Binding 1: output image WRITE
	};

	auto descriptor_layout = vk::DescriptorSetLayoutCreateInfo{};
	descriptor_layout.pBindings = set_layout_bindings.data();
	descriptor_layout.bindingCount = set_layout_bindings.size();

	VK_ASSERT(device.createDescriptorSetLayout(&descriptor_layout, nullptr, &computeResources.descriptorSetLayout));

	
	auto pipeline_layout_create_info = vk::PipelineLayoutCreateInfo{};
	pipeline_layout_create_info.pSetLayouts = &computeResources.descriptorSetLayout;
	pipeline_layout_create_info.setLayoutCount = 1;

	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_create_info, nullptr, &computeResources.pipelineLayout));


	auto allocate_info = vk::DescriptorSetAllocateInfo{};
	allocate_info.descriptorPool = descriptorPool;
	allocate_info.pSetLayouts = &computeResources.descriptorSetLayout;
	allocate_info.descriptorSetCount = 1;

	VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &computeResources.descriptorSet));


	auto compute_write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{
		createWriteDescriptorSet(computeResources.descriptorSet, vk::DescriptorType::eStorageImage, 0, &colorMap.descriptorInfo),
		createWriteDescriptorSet(computeResources.descriptorSet, vk::DescriptorType::eStorageImage, 1, &computeTarget.descriptorInfo),
	};
	device.updateDescriptorSets(compute_write_descriptor_sets.size(), compute_write_descriptor_sets.data(), 0, nullptr);

	/* Compute shader pipeline */
	/* One pipeline per effect */
	auto compute_pipeline_info = vk::ComputePipelineCreateInfo{};
	compute_pipeline_info.layout = computeResources.pipelineLayout;
	compute_pipeline_info.stage = loadShaderFromFile(device, "emboss.comp.spv", vk::ShaderStageFlagBits::eCompute);
	auto pipeline = vk::Pipeline{};
	VK_ASSERT(device.createComputePipelines(pipelineCache, 1, &compute_pipeline_info, nullptr, &pipeline));
	computeResources.pipelines.push_back(pipeline);

	auto cmd_pool_info = vk::CommandPoolCreateInfo{};
	cmd_pool_info.queueFamilyIndex = computeResources.graphicsFamilyIndex;
	cmd_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	VK_ASSERT(device.createCommandPool(&cmd_pool_info, nullptr, &computeResources.commandPool));

	/* Command buffer for compute operations */
	auto cmd_buffer_allocate_info = vk::CommandBufferAllocateInfo{};
	cmd_buffer_allocate_info.commandPool = computeResources.commandPool;
	cmd_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
	cmd_buffer_allocate_info.commandBufferCount = 1;

	VK_ASSERT(device.allocateCommandBuffers(&cmd_buffer_allocate_info, &computeResources.commandBuffer));

	/* Fence for compute synchronization */
	auto fence_info = vk::FenceCreateInfo{};
	fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
	VK_ASSERT(device.createFence(&fence_info, nullptr, &computeResources.fence));

	setupComputeCommandBuffer();
}

auto GMU::findComputeQueue() -> void
{
	uint32_t queue_family_count;
	physicalDevice.getQueueFamilyProperties(&queue_family_count, nullptr);
	assert(queue_family_count >= 1);

	auto queue_family_properties = std::vector<vk::QueueFamilyProperties>{};
	queue_family_properties.resize(queue_family_count);
	physicalDevice.getQueueFamilyProperties(&queue_family_count, queue_family_properties.data());

	auto compute_queue_found = false;
	for (uint32_t i = 0; i < static_cast<uint32_t>(queue_family_properties.size()); i++) {
		if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) && (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) == static_cast<vk::QueueFlagBits>(0)) {
			computeResources.graphicsFamilyIndex = i;
			compute_queue_found = true;
			break;
		}
	}

	if (!compute_queue_found) {
		for (uint32_t i = 0; i < static_cast<uint32_t>(queue_family_properties.size()); i++) {
			if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) {
				computeResources.graphicsFamilyIndex = i;
				compute_queue_found = true;
				break;
			}
		}
	}
	assert(compute_queue_found);

	device.getQueue(computeResources.graphicsFamilyIndex, 0, &computeResources.queue);
}

auto GMU::setupComputeCommandBuffer() -> void
{
	computeResources.queue.waitIdle();
	computeResources.commandBuffer.begin(vk::CommandBufferBeginInfo{});

	computeResources.commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computeResources.pipelines[computeResources.pipelineIndex]);
	computeResources.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computeResources.pipelineLayout, 0, 1, &computeResources.descriptorSet, 0, nullptr);

	computeResources.commandBuffer.dispatch(computeTarget.width / 16, computeTarget.height / 16, 1);
	computeResources.commandBuffer.end();
}

auto GMU::setupPipelines() -> void
{
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state = {};
	input_assembly_state.topology = vk::PrimitiveTopology::eTriangleList;
	input_assembly_state.flags = static_cast<vk::PipelineInputAssemblyStateCreateFlagBits>(0);
	input_assembly_state.primitiveRestartEnable = false;

	vk::PipelineRasterizationStateCreateInfo rasterization_state = {};
	rasterization_state.polygonMode = vk::PolygonMode::eFill;
	rasterization_state.cullMode = vk::CullModeFlagBits::eNone;
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
	depth_stencil_state.depthTestEnable = true;
	depth_stencil_state.depthWriteEnable = true;
	depth_stencil_state.depthCompareOp = vk::CompareOp::eLessOrEqual;

	vk::PipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	vk::PipelineMultisampleStateCreateInfo multisample_state = {};
	multisample_state.rasterizationSamples = vk::SampleCountFlagBits::e1;

	auto dynamic_states = std::vector<vk::DynamicState> {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo dynamic_state;
	dynamic_state.pDynamicStates = dynamic_states.data();
	dynamic_state.dynamicStateCount = dynamic_states.size();



	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	/* Compute filter shaders  */
	shader_stages[0] = loadShaderFromFile(device, "pbr_basic.vert.spv", vk::ShaderStageFlagBits::eVertex);
	shader_stages[1] = loadShaderFromFile(device, "pbr_basic.frag.spv", vk::ShaderStageFlagBits::eFragment);


	vk::GraphicsPipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.layout = graphicsResources.pipelineLayout;
	pipeline_create_info.renderPass = renderPass;
	pipeline_create_info.basePipelineHandle = nullptr;
	pipeline_create_info.basePipelineIndex = -1;

	pipeline_create_info.pVertexInputState = &vertices.inputState;
	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState = &color_blend_state;
	pipeline_create_info.pMultisampleState = &multisample_state;
	pipeline_create_info.pViewportState = &viewport_state;
	pipeline_create_info.pDepthStencilState = &depth_stencil_state;
	pipeline_create_info.pDynamicState = &dynamic_state;
	pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	pipeline_create_info.pStages = shader_stages.data();

	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &pipeline_create_info, nullptr, &graphicsResources.pipeline));
}

auto GMU::setupDescriptorPool() -> void
{
	auto pool_sizes = std::vector<vk::DescriptorPoolSize>{
		/* Graphics pipeline UBO */
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
		/* Graphics pipeline image sampler for displaying output image */
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2),
		/* Compute pipeline uses a storage image for image read/write */
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 2),
	};

	auto descriptor_pool_info = vk::DescriptorPoolCreateInfo{};
	descriptor_pool_info.pPoolSizes = pool_sizes.data();
	descriptor_pool_info.poolSizeCount = pool_sizes.size();
	descriptor_pool_info.maxSets = 3;

	VK_ASSERT(device.createDescriptorPool(&descriptor_pool_info, nullptr, &descriptorPool));
}

auto GMU::setupUniformBuffers() -> void
{
	vulkanDevice->createBuffer(
		sizeof(UBOmatrices),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffer
	);

	VK_ASSERT(uniformBuffer.map());

	updateUniformBuffers();
}

auto GMU::updateUniformBuffers() -> void
{
	uboMatrices.projection = glm::perspective(
		glm::radians(60.f),
		static_cast<float>(settings.width) * 0.5f / static_cast<float>(settings.height),
		0.1f,
		256.0f
	);
	const auto view_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));

	uboMatrices.model = view_matrix * glm::translate(glm::mat4(1.0f), cameraPos);
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboMatrices.model = glm::rotate(uboMatrices.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	memcpy(uniformBuffer.mapped, &uboMatrices, sizeof(UBOmatrices));
}

auto GMU::setupTextureTarget(vkpbr::Texture* texture_target, const uint32_t width, const uint32_t height, const vk::Format format) -> void
{
	vk::FormatProperties format_properties;
	physicalDevice.getFormatProperties(format, &format_properties);

	/* Must support image storage operations */
	assert(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eStorageImage);


	texture_target->width = width;
	texture_target->height = height;

	vk::ImageCreateInfo image_info = {};
	image_info.imageType = vk::ImageType::e2D;
	image_info.format = format;
	image_info.extent = vk::Extent3D{ width, height, 1 };
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eOptimal;
	image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage; // sampled by fragment shader & storage target in compute shader
	image_info.sharingMode = vk::SharingMode::eExclusive; // ownership does not need to be transfered explicitly between compute & graphics queue

	vk::MemoryAllocateInfo memory_allocate_info = {};
	vk::MemoryRequirements memory_requirements = {};

	VK_ASSERT(device.createImage(&image_info, nullptr, &texture_target->image));


	device.getImageMemoryRequirements(texture_target->image, &memory_requirements);
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = vulkanDevice->findMemoryType(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
	VK_ASSERT(device.allocateMemory(&memory_allocate_info, nullptr, &texture_target->deviceMemory));
	device.bindImageMemory(texture_target->image, texture_target->deviceMemory, 0);


	auto layout_command = vulkanDevice->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

	texture_target->imageLayout = vk::ImageLayout::eGeneral;
	setImageLayout(
		layout_command,
		texture_target->image,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		texture_target->imageLayout
	);

	vulkanDevice->finishAndSubmitCmdBuffer(layout_command, queue, true);

	/* Create sampler */
	vk::SamplerCreateInfo sampler = {};
	sampler.magFilter = vk::Filter::eLinear;
	sampler.minFilter = vk::Filter::eLinear;
	sampler.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler.addressModeU = vk::SamplerAddressMode::eClampToBorder;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.compareOp = vk::CompareOp::eNever;
	sampler.minLod = 0.0f;
	sampler.maxLod = texture_target->mipLevels;
	sampler.borderColor = vk::BorderColor::eFloatOpaqueWhite;
	VK_ASSERT(device.createSampler(&sampler, nullptr, &texture_target->textureSampler));

	/* Create image view */
	vk::ImageViewCreateInfo view_info = {};
	view_info.image = nullptr;
	view_info.viewType = vk::ImageViewType::e2D;
	view_info.format = format;
	view_info.components = { vk::ComponentSwizzle::eR , vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
	view_info.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
	view_info.image = texture_target->image;
	device.createImageView(&view_info, nullptr, &texture_target->imageView);

	/* Initialize descriptor */
	texture_target->descriptorInfo.imageLayout = texture_target->imageLayout;
	texture_target->descriptorInfo.imageView = texture_target->imageView;
	texture_target->descriptorInfo.sampler = texture_target->textureSampler;
	texture_target->device = vulkanDevice.get();
}

auto GMU::setupCommandBuffers() -> void
{
	/* check cmd buffers */
	if (!checkCmdBuffers()) {
		device.freeCommandBuffers(commandPool, static_cast<uint32_t>(drawCalls.size()), drawCalls.data());
		VulkanRenderer::setupCommandBuffers();
	}

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
		renderpass_info.framebuffer = framebuffers[i];

		VK_ASSERT(drawCalls[i].begin(&buffer_begin_info));

		/* image memory barrier -> make sure that compute shader finishes before sampling the texture */
		auto memory_barrier = vk::ImageMemoryBarrier{};
		memory_barrier.oldLayout = vk::ImageLayout::eGeneral;
		memory_barrier.newLayout = vk::ImageLayout::eGeneral;
		memory_barrier.image = computeTarget.image;
		memory_barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
		memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		drawCalls[i].pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eFragmentShader,
			static_cast<vk::DependencyFlagBits>(0),
			0,
			nullptr,
			0,
			nullptr,
			1,
			&memory_barrier
		);

		drawCalls[i].beginRenderPass(&renderpass_info, vk::SubpassContents::eInline);

		auto viewport = vk::Viewport{};
		viewport.width = static_cast<float>(settings.width) * 0.5f;
		viewport.height = static_cast<float>(settings.height) * 0.5f;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		drawCalls[i].setViewport(0, 1, &viewport);

		auto scissor = vk::Rect2D{};
		scissor.extent.width = settings.width;
		scissor.extent.height = settings.height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		drawCalls[i].setScissor(0, 1, &scissor);

		vk::DeviceSize offsets[1] = { 0 };
		drawCalls[i].bindVertexBuffers(0, 1, &vertexBuffer.buffer, offsets);
		drawCalls[i].bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);

		/* LEFT pre compute */
		drawCalls[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsResources.pipelineLayout, 0, 1, &graphicsResources.descriptorSetPreCompute, 0, nullptr);
		drawCalls[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsResources.pipeline);
		drawCalls[i].drawIndexed(indexCount, 1, 0, 0, 0);

		/* RIGHT post compute */
		drawCalls[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsResources.pipelineLayout, 0, 1, &graphicsResources.descriptorSetPostCompute, 0, nullptr);
		drawCalls[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsResources.pipeline);

		viewport.x = (float)settings.width / 2.0f;
		drawCalls[i].setViewport(0, 1, &viewport);
		drawCalls[i].drawIndexed(indexCount, 1, 0, 0, 0);

		// draw ui

		drawCalls[i].endRenderPass();
		drawCalls[i].end();
	}	
}

auto GMU::checkCmdBuffers() -> bool{
	for (auto& draw_call : drawCalls)
	{
		if (draw_call == static_cast<vk::CommandBuffer>(nullptr))
		{
			return false;
		}
	}
	return true;
}

auto GMU::render() -> void
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

	/* Dispatch compute */
	device.waitForFences(1, &computeResources.fence, true, UINT64_MAX);
	device.resetFences(1, &computeResources.fence);

	auto compute_submit_info = vk::SubmitInfo{};
	compute_submit_info.commandBufferCount = 1;
	compute_submit_info.pCommandBuffers = &computeResources.commandBuffer;
	VK_ASSERT(computeResources.queue.submit(1, &compute_submit_info, computeResources.fence));
}

auto GMU::updateView() -> void
{
	updateUniformBuffers();
}

auto GMU::updateUI(const uint32_t object_index) -> void
{

}
