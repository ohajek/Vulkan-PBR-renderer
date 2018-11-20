#pragma once

#include <iostream>
#include <fstream>
#include <vulkan/vulkan.hpp>

#define VK_CHECK_RESULT(f)																				\
{																										\
	vk::Result res = (f);																				\
	if (res != vk::Result::eSuccess)																	\
	{																									\
		std::cout << "[FATAL]: Vulkan fn returned \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == vk::Result::eSuccess);															\
	}																									\
}

#define VK_CHECK_RESULT_C(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "[FATAL]: Vulkan fn returned \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

#define VK_CHECK_RESULT_VALUE(f)																				\
{																										\
	vk::ResultValueType<void> res = (f);																					\
	if (res.result != vk::Result::eSuccess)																				\
	{																									\
		std::cout << "[FATAL]: Vulkan fn returned \"" << res.result << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res.result == vk::Result::eSuccess);																		\
	}																									\
}


inline auto loadShaderFromFile(vk::Device& device, const std::string& filename, const vk::ShaderStageFlagBits shader_stage) -> vk::PipelineShaderStageCreateInfo
{
	vk::PipelineShaderStageCreateInfo shader_stage_info = {};
	shader_stage_info.stage = shader_stage;
	shader_stage_info.pName = "main";

	auto input_stream = std::ifstream(std::string(SHADER_DIR) + filename, std::ios::binary | std::ios::in | std::ios::ate);

	if (input_stream.is_open()) {
		const auto filesize = input_stream.tellg();
		input_stream.seekg(0, std::ios::beg);
		auto* shader_code = new char[filesize];

		input_stream.read(shader_code, filesize);
		input_stream.close();

		assert(filesize > 0);

		vk::ShaderModuleCreateInfo module_create_info = {};
		module_create_info.codeSize = filesize;
		module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_code);

		device.createShaderModule(&module_create_info, nullptr, &shader_stage_info.module);

		delete[] shader_code;
	}
	else {
		std::cerr << "[ERROR]: Could not open shader: " << filename << std::endl;
		exit(EXIT_FAILURE);
	}
	return shader_stage_info;
}