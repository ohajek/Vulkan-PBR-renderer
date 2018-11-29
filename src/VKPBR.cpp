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

auto VKPBR::render() -> void
{
	if (!preparedToRender) {
		return;
	}
}