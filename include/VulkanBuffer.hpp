#pragma once
#include <vulkan/vulkan.hpp>

namespace vkpbr {
	
	struct Buffer {
		vk::Device device;
		vk::Buffer buffer = nullptr;
		vk::DeviceMemory memory = nullptr;
		vk::DescriptorBufferInfo descriptor;
		vk::DeviceSize size = 0;
		vk::DeviceSize alignment = 0;
		void* mapped = nullptr;

		vk::BufferUsageFlags usageFlags;
		vk::MemoryPropertyFlags memoryPropertyFlags;


		auto map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) -> vk::Result
		{
			return device.mapMemory(memory, offset, size, static_cast<vk::MemoryMapFlagBits>(0), &mapped);
		}

		auto unmap() -> void
		{
			if (mapped)
			{
				device.unmapMemory(memory);
				mapped = nullptr;
			}
		}

		auto bind(vk::DeviceSize offset = 0) const -> void
		{
			device.bindBufferMemory(buffer, memory, offset);
		}

		auto setupDescriptor(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) -> void
		{
			descriptor.offset = offset;
			descriptor.buffer = buffer;
			descriptor.range = size;
		}

		auto copyTo(void* data, vk::DeviceSize size) -> void
		{
			assert(mapped);
			memcpy(mapped, data, size);
		}

		auto flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> vk::Result
		{
			vk::MappedMemoryRange mappedRange = {};
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return device.flushMappedMemoryRanges(1, &mappedRange);
		}

		auto invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) -> vk::Result
		{
			vk::MappedMemoryRange mappedRange = {};
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return device.invalidateMappedMemoryRanges(1, &mappedRange);
		}

		auto destroy() -> void
		{
			if (buffer) {
				device.destroyBuffer(buffer, nullptr);
			}
			if (memory) {
				device.freeMemory(memory, nullptr);
			}
		}
	};
}
