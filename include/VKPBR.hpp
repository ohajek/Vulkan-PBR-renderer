#pragma once

#include <VulkanRenderer.hpp>
#include <VulkanTexture.hpp>

class VKPBR : public VulkanRenderer
{
public:
	VKPBR();

	~VKPBR() override;

	auto prepareForRender() -> void override;

	auto render() -> void override;
};
