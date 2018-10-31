#pragma once

#include <iostream>
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