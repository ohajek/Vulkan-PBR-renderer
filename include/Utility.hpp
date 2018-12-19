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

		VK_ASSERT(device.createShaderModule(&module_create_info, nullptr, &shader_stage_info.module));

		delete[] shader_code;
	}
	else {
		std::cerr << "[ERROR]: Could not open shader: " << filename << std::endl;
		exit(EXIT_FAILURE);
	}
	return shader_stage_info;
}

inline auto setImageLayout(
	vk::CommandBuffer cmd_buffer,
	vk::Image image,
	const vk::ImageLayout old_layout,
	const vk::ImageLayout new_layout,
	const vk::ImageSubresourceRange subresource_range,
	const vk::PipelineStageFlagBits src_stage_mask = vk::PipelineStageFlagBits::eAllCommands,
	const vk::PipelineStageFlagBits dst_stage_mask = vk::PipelineStageFlagBits::eAllCommands) -> void
{
	/* Memory barrier begin */
	vk::ImageMemoryBarrier memory_barrier = {};
	memory_barrier.oldLayout = old_layout;
	memory_barrier.newLayout = new_layout;
	memory_barrier.image = image;
	memory_barrier.subresourceRange = subresource_range;

	switch (old_layout) {
	case vk::ImageLayout::eUndefined:
		memory_barrier.srcAccessMask = static_cast<vk::AccessFlagBits>(0);
		break;

	case vk::ImageLayout::ePreinitialized:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
		break;

	case vk::ImageLayout::eColorAttachmentOptimal:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;

	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		break;

	case vk::ImageLayout::eTransferSrcOptimal:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		break;

	case vk::ImageLayout::eTransferDstOptimal:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;

	case vk::ImageLayout::eShaderReadOnlyOptimal:
		memory_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		break;

	default:
		break;
	}

	switch (new_layout) {
	case vk::ImageLayout::eTransferDstOptimal:
		memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;

	case vk::ImageLayout::eTransferSrcOptimal:
		memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
		break;

	case vk::ImageLayout::eColorAttachmentOptimal:
		memory_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;

	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		memory_barrier.dstAccessMask = memory_barrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		break;

	case vk::ImageLayout::eShaderReadOnlyOptimal:
		if (memory_barrier.srcAccessMask == static_cast<vk::AccessFlagBits>(0)) {
			memory_barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
		}
		memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		break;

	default:
		break;
	}

	cmd_buffer.pipelineBarrier(
		src_stage_mask,
		dst_stage_mask,
		static_cast<vk::DependencyFlagBits>(0),
		0,
		nullptr,
		0, nullptr,
		1, &memory_barrier
	);
}

inline auto setImageLayout(
	const vk::CommandBuffer cmd_buffer,
	const vk::Image image,
	const vk::ImageAspectFlagBits aspect_mask,
	const vk::ImageLayout old_layout,
	const vk::ImageLayout new_layout,
	const vk::PipelineStageFlagBits src_stage_mask = vk::PipelineStageFlagBits::eAllCommands,
	const vk::PipelineStageFlagBits dst_stage_mask = vk::PipelineStageFlagBits::eAllCommands) -> void
{
	vk::ImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = aspect_mask;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = 1;
	subresource_range.layerCount = 1;
	setImageLayout(cmd_buffer, image, old_layout, new_layout, subresource_range, src_stage_mask, dst_stage_mask);
}

inline auto createWriteDescriptorSet(
	const vk::DescriptorSet dst_set,
	const vk::DescriptorType type,
	const uint32_t binding,
	vk::DescriptorImageInfo* image_info,
	const uint32_t descriptor_count = 1) -> vk::WriteDescriptorSet
{
	vk::WriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.dstSet = dst_set;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pImageInfo = image_info;
	writeDescriptorSet.descriptorCount = descriptor_count;
	return writeDescriptorSet;
}

inline auto createWriteDescriptorSet(
	const vk::DescriptorSet dst_set,
	const vk::DescriptorType type,
	const uint32_t binding,
	vk::DescriptorBufferInfo* buffer_info,
	const uint32_t descriptor_count = 1) -> vk::WriteDescriptorSet
{
	vk::WriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.dstSet = dst_set;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pBufferInfo = buffer_info;
	writeDescriptorSet.descriptorCount = descriptor_count;
	return writeDescriptorSet;
}