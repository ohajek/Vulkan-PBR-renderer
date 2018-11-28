#pragma once

#include <iostream>
#include <fstream>
#include <vulkan/vulkan.hpp>



inline std::string result_to_string(vk::Result value)
{
	switch (value)
	{
	case vk::Result::eSuccess: return "Success";
	case vk::Result::eNotReady: return "NotReady";
	case vk::Result::eTimeout: return "Timeout";
	case vk::Result::eEventSet: return "EventSet";
	case vk::Result::eEventReset: return "EventReset";
	case vk::Result::eIncomplete: return "Incomplete";
	case vk::Result::eErrorOutOfHostMemory: return "ErrorOutOfHostMemory";
	case vk::Result::eErrorOutOfDeviceMemory: return "ErrorOutOfDeviceMemory";
	case vk::Result::eErrorInitializationFailed: return "ErrorInitializationFailed";
	case vk::Result::eErrorDeviceLost: return "ErrorDeviceLost";
	case vk::Result::eErrorMemoryMapFailed: return "ErrorMemoryMapFailed";
	case vk::Result::eErrorLayerNotPresent: return "ErrorLayerNotPresent";
	case vk::Result::eErrorExtensionNotPresent: return "ErrorExtensionNotPresent";
	case vk::Result::eErrorFeatureNotPresent: return "ErrorFeatureNotPresent";
	case vk::Result::eErrorIncompatibleDriver: return "ErrorIncompatibleDriver";
	case vk::Result::eErrorTooManyObjects: return "ErrorTooManyObjects";
	case vk::Result::eErrorFormatNotSupported: return "ErrorFormatNotSupported";
	case vk::Result::eErrorFragmentedPool: return "ErrorFragmentedPool";
	case vk::Result::eErrorSurfaceLostKHR: return "ErrorSurfaceLostKHR";
	case vk::Result::eErrorNativeWindowInUseKHR: return "ErrorNativeWindowInUseKHR";
	case vk::Result::eSuboptimalKHR: return "SuboptimalKHR";
	case vk::Result::eErrorOutOfDateKHR: return "ErrorOutOfDateKHR";
	case vk::Result::eErrorIncompatibleDisplayKHR: return "ErrorIncompatibleDisplayKHR";
	case vk::Result::eErrorValidationFailedEXT: return "ErrorValidationFailedEXT";
	case vk::Result::eErrorInvalidShaderNV: return "ErrorInvalidShaderNV";
	case vk::Result::eErrorOutOfPoolMemoryKHR: return "ErrorOutOfPoolMemoryKHR";
	case vk::Result::eErrorInvalidExternalHandleKHR: return "ErrorInvalidExternalHandleKHR";
	case vk::Result::eErrorNotPermittedEXT: return "ErrorNotPermittedEXT";
	default: return "invalid";
	}
}



#define VK_ASSERT(f)																				\
{																										\
	vk::Result res = (f);																				\
	if (res != vk::Result::eSuccess)																	\
	{																									\
		std::cout << "[FATAL]: Vulkan returned \"" << result_to_string(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == vk::Result::eSuccess);															\
	}																									\
}

#define VK_ASSERT_C(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "[FATAL]: Vulkan returned \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

#define VK_ASSERT_RESULT_VALUE(f)																				\
{																										\
	vk::ResultValueType<void> res = (f);																					\
	if (res.result != vk::Result::eSuccess)																				\
	{																									\
		std::cout << "[FATAL]: Vulkan returned \"" << res.result << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
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