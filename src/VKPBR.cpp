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
	setupUniformBuffers();
	setupDescriptors();
	setupPipelines();
	createCommandBuffers();

	preparedToRender = true;
}

auto VKPBR::setupPipelines() -> void
{
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
	input_assembly_state_create_info.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
	rasterization_state_create_info.polygonMode = vk::PolygonMode::eFill;
	rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
	rasterization_state_create_info.frontFace = vk::FrontFace::eCounterClockwise;
	rasterization_state_create_info.lineWidth = 1.0f;

	vk::PipelineColorBlendAttachmentState blend_attachment_state = {};
	blend_attachment_state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blend_attachment_state.blendEnable = false;

	vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
	color_blend_state_create_info.attachmentCount = 1;
	color_blend_state_create_info.pAttachments = &blend_attachment_state;

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
	depth_stencil_state_create_info.depthWriteEnable = false;
	depth_stencil_state_create_info.depthTestEnable = false;
	depth_stencil_state_create_info.depthCompareOp = vk::CompareOp::eLessOrEqual;
	depth_stencil_state_create_info.front = depth_stencil_state_create_info.back;
	depth_stencil_state_create_info.back.compareOp = vk::CompareOp::eAlways;

	vk::PipelineViewportStateCreateInfo viewport_state_create_info = {};
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.scissorCount = 1;

	vk::PipelineMultisampleStateCreateInfo multisample_state_create_info = {};

	auto dynamic_state_enables = std::vector<vk::DynamicState> {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	vk::PipelineDynamicStateCreateInfo dynamic_state_create_info = {};
	dynamic_state_create_info.pDynamicStates = dynamic_state_enables.data();
	dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables.size());

	/* Pipeline layout */
	const auto descriptor_set_layouts = std::vector<vk::DescriptorSetLayout> {
		descriptorSetLayouts.scene,
		descriptorSetLayouts.material,
		descriptorSetLayouts.node
	};

	vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
	pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();

	vk::PushConstantRange push_constant_range = {};
	push_constant_range.size = sizeof(PushConstantBlockMaterial);
	push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
	VK_ASSERT(device.createPipelineLayout(&pipeline_layout_create_info, nullptr, &pipelineLayout));

	/* Vertex binding */
	vk::VertexInputBindingDescription vertex_input_binding = { 0, sizeof(vkpbr::gltf::Model::Vertex), vk::VertexInputRate::eVertex };
	auto vertex_input_attributes = std::vector<vk::VertexInputAttributeDescription> {
		{ 0, 0, vk::Format::eR32G32B32Sfloat, 0 },
		{ 1, 0, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3 },
		{ 2, 0, vk::Format::eR32G32Sfloat, sizeof(float) * 6 }
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

	/* Skybox pipeline */


	/* PBR pipeline*/
	shader_stages = {
		loadShaderFromFile(device, "pbr_shader.vert.spv", vk::ShaderStageFlagBits::eVertex),
		loadShaderFromFile(device, "pbr_shader.frag.spv", vk::ShaderStageFlagBits::eFragment)
	};

	depth_stencil_state_create_info.depthWriteEnable = true;
	depth_stencil_state_create_info.depthTestEnable = true;
	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &graphics_pipeline_create_info, nullptr, &pipelines.pbr));

	/* PBR blending */
	rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eNone;
	blend_attachment_state.blendEnable = true;
	blend_attachment_state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blend_attachment_state.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	blend_attachment_state.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	blend_attachment_state.colorBlendOp = vk::BlendOp::eAdd;
	blend_attachment_state.srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	blend_attachment_state.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	blend_attachment_state.alphaBlendOp = vk::BlendOp::eAdd;
	VK_ASSERT(device.createGraphicsPipelines(pipelineCache, 1, &graphics_pipeline_create_info, nullptr, &pipelines.pbrAlphaBlend));

	for (auto& shader_stage : shader_stages) {
		device.destroyShaderModule(shader_stage.module, nullptr);
	}
}

auto VKPBR::setupDescriptors() -> void
{
	uint32_t image_sampler_count = 0;
	uint32_t material_count = 0;
	uint32_t mesh_count = 0;

	image_sampler_count += 3;

	auto model_list = std::vector<vkpbr::gltf::Model*>{ &models.scene }; // add skybox
	for (auto& model : model_list) {
		for (auto& material : model->materials) {
			image_sampler_count += 5;
			material_count++;
		}
		for (auto& node : model->linearNodes) {
			if (node->mesh) {
				mesh_count++;
			}
		}
	}

	auto pool_sizes = std::vector<vk::DescriptorPoolSize> {
		{ vk::DescriptorType::eUniformBuffer, 4 + mesh_count },
		{ vk::DescriptorType::eCombinedImageSampler, image_sampler_count}
	};

	vk::DescriptorPoolCreateInfo descriptor_pool_create_info = {};
	descriptor_pool_create_info.poolSizeCount = 2;
	descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
	descriptor_pool_create_info.maxSets = 1 + material_count + mesh_count; //TODO: tady mozna dva
	VK_ASSERT(device.createDescriptorPool(&descriptor_pool_create_info, nullptr, &descriptorPool));



	/* Scene descriptor set (matrices and environment map) */
	{
		auto set_layout_bindings = std::vector<vk::DescriptorSetLayoutBinding> {
			{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr }
			//{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			//{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			//{ 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			//{ 4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
		};

		vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
		descriptor_set_layout_create_info.pBindings = set_layout_bindings.data();
		descriptor_set_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
		VK_ASSERT(device.createDescriptorSetLayout(&descriptor_set_layout_create_info, nullptr, &descriptorSetLayouts.scene));

		vk::DescriptorSetAllocateInfo allocate_info = {};
		allocate_info.descriptorPool = descriptorPool;
		allocate_info.pSetLayouts = &descriptorSetLayouts.scene;
		allocate_info.descriptorSetCount = 1;
		VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &descriptorSets.scene));

		auto write_descriptor_sets = std::array<vk::WriteDescriptorSet, 1> {};

		write_descriptor_sets[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].dstSet = descriptorSets.scene;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].pBufferInfo = &uniformBuffers.scene.descriptor;
		/*
		write_descriptor_sets[1].descriptorType = vk::DescriptorType::eUniformBuffer;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].dstSet = descriptorSets.scene;
		write_descriptor_sets[1].dstBinding = 1;
		write_descriptor_sets[1].pBufferInfo = &uniformBuffers.parameters.descriptor;
		*/

		device.updateDescriptorSets(static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
	}

	/* Material samplers */
	{
		auto set_layout_bindings = std::vector<vk::DescriptorSetLayoutBinding> {
			{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			{ 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
			{ 4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr },
		};

		vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
		descriptor_set_layout_create_info.pBindings = set_layout_bindings.data();
		descriptor_set_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
		VK_ASSERT(device.createDescriptorSetLayout(&descriptor_set_layout_create_info, nullptr, &descriptorSetLayouts.material));

		for (auto& material : models.scene.materials) {
			vk::DescriptorSetAllocateInfo allocate_info = {};
			allocate_info.descriptorPool = descriptorPool;
			allocate_info.pSetLayouts = &descriptorSetLayouts.material;
			allocate_info.descriptorSetCount = 1;
			VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &material.descriptorSet));

			auto image_descriptors = std::vector<vk::DescriptorImageInfo>{
				textures.empty.descriptorInfo,
				textures.empty.descriptorInfo,
				material.normalTexture ? material.normalTexture->descriptorInfo : textures.empty.descriptorInfo,
				material.occlusionTexture ? material.occlusionTexture->descriptorInfo : textures.empty.descriptorInfo,
				material.emissiveTexture ? material.emissiveTexture->descriptorInfo : textures.empty.descriptorInfo
			};
			
			if (material.pbrWorkflows.metallicRoughness) {
				if (material.baseColorTexture) {
					image_descriptors[0] = material.baseColorTexture->descriptorInfo;
				}
				if (material.metallicRoughnessTexture) {
					image_descriptors[1] = material.metallicRoughnessTexture->descriptorInfo;
				}
			}

			if (material.pbrWorkflows.specularGlossiness) {
				if (material.extension.diffuseTexture) {
					image_descriptors[0] = material.extension.diffuseTexture->descriptorInfo;
				}
				if (material.extension.specularGlossinessTexture) {
					image_descriptors[1] = material.extension.specularGlossinessTexture->descriptorInfo;
				}
			}

			auto write_descriptor_sets = std::array<vk::WriteDescriptorSet, 5> {};
			for(size_t i = 0; i < image_descriptors.size(); i++) {
				write_descriptor_sets[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
				write_descriptor_sets[i].descriptorCount = 1;
				write_descriptor_sets[i].dstSet = material.descriptorSet;
				write_descriptor_sets[i].dstBinding = static_cast<uint32_t>(i);
				write_descriptor_sets[i].pImageInfo = &image_descriptors[i];
			}

			device.updateDescriptorSets(static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
		}
		
		/* Model node (matrices) */
		{
			auto set_layout_bindings = std::vector<vk::DescriptorSetLayoutBinding> {
				{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr },
			};

			vk::DescriptorSetLayoutCreateInfo set_layout_create_info = {};
			set_layout_create_info.pBindings = set_layout_bindings.data();
			set_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
			VK_ASSERT(device.createDescriptorSetLayout(&set_layout_create_info, nullptr, &descriptorSetLayouts.node));

			/* per node desc. set */
			for (auto& node : models.scene.nodes) {
				setupNodeDescriptorSet(node);
			}
		}

		/* Skybox */
	}
}

auto VKPBR::setupUniformBuffers() -> void
{
	/* object vertex shader uniform buffer */
	VK_ASSERT(vulkanDevice->createBuffer(
		sizeof(uboMatrices),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffers.scene.buffer,
		uniformBuffers.scene.memory
	));

	/* skybox vertex shader uniform buffer */

	/* Shared parameters */
	VK_ASSERT(vulkanDevice->createBuffer(
		sizeof(uboParameters),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniformBuffers.parameters.buffer,
		uniformBuffers.parameters.memory
	));

	uniformBuffers.scene.descriptor = vk::DescriptorBufferInfo{ uniformBuffers.scene.buffer, 0, sizeof(uboMatrices) };
	//uniformBuffers.skybox.descriptor = { uniformBuffers.scene.buffer, 0, sizeof(uboMatrices) };
	uniformBuffers.parameters.descriptor = vk::DescriptorBufferInfo{ uniformBuffers.parameters.buffer, 0, sizeof(uboParameters) };

	VK_ASSERT(device.mapMemory(uniformBuffers.scene.memory, 0, sizeof(uboMatrices), static_cast<vk::MemoryMapFlags>(0), &uniformBuffers.scene.mappedMemory));
	// map skybox
	VK_ASSERT(device.mapMemory(uniformBuffers.parameters.memory, 0, sizeof(uboParameters), static_cast<vk::MemoryMapFlags>(0), &uniformBuffers.parameters.mappedMemory));

	updateUniformBuffers();
	updateParameters();
}

auto VKPBR::updateUniformBuffers() -> void
{
	/* Scene */
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

	/* skybox */
	/*
	uboMatrices.model = glm::mat4(glm::mat3(camera.matrices.view));
	memcpy(uniformBuffers.skybox.mappedMemory, &uboMatrices, sizeof(uboMatrices));
	 */
}

auto VKPBR::updateParameters() -> void
{
	uboParameters.lightDirection = glm::vec4(
		sin(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
		sin(glm::radians(lightSource.rotation.y)),
		cos(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
		0.0f
	);
	memcpy(uniformBuffers.parameters.mappedMemory, &uboParameters, sizeof(uboParameters));
}

auto VKPBR::render() -> void
{
	if (!preparedToRender) {
		return;
	}

	VulkanRenderer::prepareFrame();
	VK_ASSERT(device.waitForFences(1, &memoryFences[currentBuffer], VK_TRUE, UINT64_MAX));
	VK_ASSERT(device.resetFences(1, &memoryFences[currentBuffer]));

	const vk::PipelineStageFlags wait_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::SubmitInfo submit_info = {};
	submit_info.pWaitDstStageMask = &wait_stage_mask;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &presentCompleteSemaphore;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &renderCompleteSemaphore;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &drawCalls[currentBuffer];
	VK_ASSERT(queue.submit(1, &submit_info, memoryFences[currentBuffer])); //TODO: sem me hodi debugger


	VulkanRenderer::submitFrame();

	if (rotateModel) {
		modelRotation.y += stopwatch.delta * 35.0f;
		if (modelRotation.y > 360.0f) {
			modelRotation.y -= 360.0f;
		}
	}

	updateParameters();
	if (rotateModel) {
		updateUniformBuffers();
	}

	if (camera.updated) {
		updateUniformBuffers();
	}
}

auto VKPBR::renderGLTFNode(vkpbr::gltf::Node* node, vk::CommandBuffer cmd_buffer, const vkpbr::gltf::Material::AlphaMode alpha_mode) const -> void
{
	if (node->mesh) {
		for (auto* primitive : node->mesh->primitives) {
			if (primitive->material.alphaMode == alpha_mode) {
				
				const auto descriptor_sets = std::vector<vk::DescriptorSet>{
					descriptorSets.scene,
					primitive->material.descriptorSet,
					node->mesh->uniformBuffer.descriptorSet
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

				/* Passing material parameters as push constants */
				PushConstantBlockMaterial push_constant_block = {};
				push_constant_block.hasEmissiveTexture = static_cast<float>(primitive->material.emissiveTexture != nullptr);
				push_constant_block.emissiveFactor = primitive->material.emissiveFactor;
				push_constant_block.hasNormalTexture = static_cast<float>(primitive->material.normalTexture != nullptr);
				push_constant_block.hasOcclusionTexture = static_cast<float>(primitive->material.occlusionTexture != nullptr);
				push_constant_block.alphaMask = static_cast<float>(primitive->material.alphaMode == vkpbr::gltf::Material::AlphaMode::mask);
				push_constant_block.alphaMaskCutoff = primitive->material.alphaCutoff;

				/* Metallic roughness workflow */
				if (primitive->material.pbrWorkflows.metallicRoughness) {
					push_constant_block.workflow = static_cast<float>(PBRworkflow::metallic_roughness);
					push_constant_block.baseColorFactor = primitive->material.baseColorFactor;
					push_constant_block.metallicFactor = primitive->material.metallicFactor;
					push_constant_block.roughnessFactor = primitive->material.roughnessFactor;
					push_constant_block.hasPhysicalDescriptorTexture = static_cast<float>(primitive->material.metallicRoughnessTexture != nullptr);
					push_constant_block.hasColorTexture = static_cast<float>(primitive->material.baseColorTexture != nullptr);
				}

				/* Specular glosiness workflow */
				if (primitive->material.pbrWorkflows.specularGlossiness) {
					push_constant_block.workflow = static_cast<float>(PBRworkflow::specular_glosiness);
					push_constant_block.hasPhysicalDescriptorTexture = static_cast<float>(primitive->material.extension.specularGlossinessTexture != nullptr);
					push_constant_block.hasColorTexture = static_cast<float>(primitive->material.extension.diffuseTexture != nullptr);
					push_constant_block.diffuseFactor = primitive->material.extension.diffuseFactor;
					push_constant_block.specularFactor = glm::vec4(primitive->material.extension.specularFactor, 1.0f);
				}

				cmd_buffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstantBlockMaterial), &push_constant_block);
				cmd_buffer.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
	}

	for (auto& child : node->children) {
		renderGLTFNode(child, cmd_buffer, alpha_mode);
	}
}

auto VKPBR::createCommandBuffers() -> void
{
	vk::CommandBufferBeginInfo buffer_info;

	vk::ClearValue clear_values[2];
	clear_values[0].color = vk::ClearColorValue(std::array<float, 4> {1.0f, 1.0f, 0.0f, 1.0f} );
	clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderPassBeginInfo render_pass_begin_info = {};
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = settings.width;
	render_pass_begin_info.renderArea.extent.height = settings.height;
	render_pass_begin_info.clearValueCount = 2;
	render_pass_begin_info.pClearValues = clear_values;

	for (size_t i = 0; i < drawCalls.size(); ++i) {
		render_pass_begin_info.framebuffer = framebuffers[i];

		VK_ASSERT(drawCalls[i].begin(&buffer_info));
		drawCalls[i].beginRenderPass(&render_pass_begin_info, vk::SubpassContents::eInline);

		vk::Viewport viewport = {};
		viewport.width = static_cast<float>(settings.width);
		viewport.height = static_cast<float>(settings.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		drawCalls[i].setViewport(0, 1, &viewport);

		vk::Rect2D scissor = {};
		scissor.extent = vk::Extent2D{ settings.width, settings.height };
		drawCalls[i].setScissor(0, 1, &scissor);

		vk::DeviceSize offsets[1] = { 0 };

		/* First render skybox */
		//drawCall.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, nullptr);
		//drawCall.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.skybox);
		//models.skybox.draw(drawCall);

		/* Render PBR scene */
		drawCalls[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.pbr);
		auto& model = models.scene;

		drawCalls[i].bindVertexBuffers(0, 1, &model.vertices.buffer, offsets);
		drawCalls[i].bindIndexBuffer(model.indices.buffer, 0, vk::IndexType::eUint32);

		/* Render opaque meshes first */
		for (auto node : model.nodes) {
			renderGLTFNode(node, drawCalls[i], vkpbr::gltf::Material::AlphaMode::opaque);
		}
		
		/* Then transparent */
		/*
		drawCall.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.pbrAlphaBlend);
		for (auto& node : model.nodes) {
			renderGLTFNode(node, drawCall, vkpbr::gltf::Material::AlphaMode::mask);
		}
		for (auto& node : model.nodes) {
			renderGLTFNode(node, drawCall, vkpbr::gltf::Material::AlphaMode::blend);
		}*/

		drawCalls[i].endRenderPass();
		drawCalls[i].end();
	}
}

auto VKPBR::loadAssets() -> void
{
	const auto& resource_path = std::string(RESOURCE_DIR);
	textures.empty.loadFromFile(resource_path + "textures/empty.ktx", vk::Format::eR8G8B8A8Unorm, vulkanDevice.get(), queue);

	const auto& test_scene_file = resource_path + "models/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf";
	const auto& env_map_file = resource_path + "textures/papermill_hdr16f_cube.ktx";

	uboMatrices.flipUV = 1.0f;

	models.scene.loadFromFile(test_scene_file, vulkanDevice.get(), queue);

	scale = 1.0f / models.scene.dimensions.radius;
	camera.setPosition(glm::vec3(-models.scene.dimensions.center.x * scale, -models.scene.dimensions.center.y * scale, camera.position.z));
}

auto VKPBR::setupNodeDescriptorSet(vkpbr::gltf::Node* node) const -> void
{
	if (node->mesh) {
		vk::DescriptorSetAllocateInfo allocate_info = {};
		allocate_info.descriptorPool = descriptorPool;
		allocate_info.pSetLayouts = &descriptorSetLayouts.node;
		allocate_info.descriptorSetCount = 1;
		VK_ASSERT(device.allocateDescriptorSets(&allocate_info, &node->mesh->uniformBuffer.descriptorSet));

		vk::WriteDescriptorSet write_descriptor_set = {};
		write_descriptor_set.descriptorType = vk::DescriptorType::eUniformBuffer;
		write_descriptor_set.descriptorCount = 1;
		write_descriptor_set.dstSet = node->mesh->uniformBuffer.descriptorSet;
		write_descriptor_set.dstBinding = 0;
		write_descriptor_set.pBufferInfo = &node->mesh->uniformBuffer.descriptor;

		device.updateDescriptorSets(1, &write_descriptor_set, 0, nullptr);
	}

	for (auto& child : node->children) {
		setupNodeDescriptorSet(child);
	}
}