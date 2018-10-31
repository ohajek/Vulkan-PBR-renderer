#include <VKPBR.hpp>

VKPBR::VKPBR()
{
	windowTitle = "vkPBR renderer";
}

VKPBR::~VKPBR()
{
}

auto VKPBR::prepareForRender() -> void
{
	VulkanRenderer::prepareForRender();
}

auto VKPBR::render() -> void
{
	
}
