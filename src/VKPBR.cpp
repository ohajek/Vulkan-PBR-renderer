#include <VKPBR.hpp>

VKPBR::VKPBR()
{
	windowTitle = "vkPBR renderer";
}

VKPBR::~VKPBR() = default;

auto VKPBR::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
	/*
	vkpbr::Texture2D empty;
	const auto filepath = std::string(RESOURCE_DIR);
	empty.loadFromFile(filepath + "/empty.ktx", vk::Format::eR8G8B8A8Unorm, vulkanDevice.get(), queue);
	empty.release();
	*/
}

auto VKPBR::render() -> void
{
	
}
