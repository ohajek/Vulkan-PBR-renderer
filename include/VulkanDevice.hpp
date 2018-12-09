#pragma once

#include <cassert>
#include <vector>
#include <set>
#include <optional>

#include <vulkan/vulkan.hpp>

#include <Utility.hpp>
#include <VulkanBuffer.hpp>
#include <CustomException.hpp>


namespace vkpbr
{
	using vkpbr::CustomException;
	class VulkanDeviceException : public CustomException {
		using CustomException::CustomException;
	};


	class VulkanDevice
	{
	public:
		vk::PhysicalDevice                        physicalDevice;
		vk::Device                                logicalDevice;
		vk::PhysicalDeviceProperties              deviceProperties;
		vk::PhysicalDeviceFeatures                deviceFeatures;
		vk::PhysicalDeviceFeatures                enabledFeatures;
		vk::PhysicalDeviceMemoryProperties        deviceMemoryProperties;
		std::vector<vk::QueueFamilyProperties>    queueFamilyProperties;
		std::vector<std::string>                  supportedExtensions;
		vk::CommandPool                           commandPool;
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			//std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> computeFamily;

			auto isComplete() const -> bool
			{
				return graphicsFamily.has_value()
					//&& presentFamily.has_value()
					&& computeFamily.has_value();
			}
		} queueFamilyIndices;

		/*operator vk::Device()
		{
			return logicalDevice;
		}*/

		VulkanDevice() {};

		VulkanDevice(vk::PhysicalDevice device)
			: physicalDevice(device)
		{
			physicalDevice.getProperties(&deviceProperties);
			physicalDevice.getFeatures(&deviceFeatures);
			physicalDevice.getMemoryProperties(&deviceMemoryProperties);

			/* Get queue family properties data */
			uint32_t queue_family_count;
			physicalDevice.getQueueFamilyProperties(&queue_family_count, nullptr);
			assert(queue_family_count > 0);
			queueFamilyProperties.resize(queue_family_count);
			physicalDevice.getQueueFamilyProperties(&queue_family_count, queueFamilyProperties.data());
		}

		~VulkanDevice()
		{
			if (commandPool) {
				logicalDevice.destroyCommandPool(commandPool, nullptr);
			}

			if (logicalDevice)
			{
				logicalDevice.destroy(nullptr);
			}
		}

		auto findMemoryType(const uint32_t type_filter, const vk::MemoryPropertyFlags& properties) const -> uint32_t {
			/*
			If there is a memory type suitable for the buffer that also has
			all of the properties we need, then we return its index
			*/
			for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
				if ((type_filter & (1 << i)) && (deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}
			throw VulkanDeviceException("[ERROR] Failed to find matching memory type.");
		}

		auto getQueueFamilyIndex(const vk::QueueFlagBits queue_flags) -> uint32_t
		{
			//Looking for queue family index supporting compute but not graphics
			if (queue_flags == vk::QueueFlagBits::eCompute) {
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
					if ((queueFamilyProperties[i].queueFlags & queue_flags) && (!(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics))) {
						return i;
						break;
					}
				}
			}

			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
				if (queueFamilyProperties[i].queueFlags & queue_flags) {
					return i;
					break;
				}
			}

			throw VulkanDeviceException("[EXCEPTION] Could not find matching queue family index.");
		}

		auto createCommandBuffer(const vk::CommandBufferLevel buffer_level, const bool begin_record = false) const -> vk::CommandBuffer
		{
			vk::CommandBufferAllocateInfo alloc_info = {};
			alloc_info.commandPool = commandPool;
			alloc_info.level = buffer_level;
			alloc_info.commandBufferCount = 1;

			vk::CommandBuffer cmd_buffer;
			if (vk::Result::eSuccess != logicalDevice.allocateCommandBuffers(&alloc_info, &cmd_buffer)) {
				throw VulkanDeviceException("[EXCEPTION] Failed to create command buffer.");
			}

			//if requested, start recording
			if (begin_record) {
				vk::CommandBufferBeginInfo begin_info = {};
				cmd_buffer.begin(&begin_info); // TODO: error checking
			}

			return cmd_buffer;
		}

		auto finishAndSubmitCmdBuffer(vk::CommandBuffer cmd_buffer, vk::Queue queue, const bool should_free_buffer = true) const -> void
		{
			cmd_buffer.end();

			vk::SubmitInfo submit_info = {};
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &cmd_buffer;

			vk::FenceCreateInfo fence_create_info = {};
			vk::Fence finished_executing_fence;
			VK_ASSERT(logicalDevice.createFence(&fence_create_info, nullptr, &finished_executing_fence));

			VK_ASSERT(queue.submit(1, &submit_info, finished_executing_fence));
			VK_ASSERT(logicalDevice.waitForFences(1, &finished_executing_fence, VK_TRUE, 100000000000));

			logicalDevice.destroyFence(finished_executing_fence, nullptr);

			if (should_free_buffer) {
				logicalDevice.freeCommandBuffers(commandPool, 1, &cmd_buffer);
			}
		}

		auto createCommandPool(
			const uint32_t queue_family_index,
			const vk::CommandPoolCreateFlags& create_flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const -> VkCommandPool
		{
			vk::CommandPoolCreateInfo create_info = {};
			create_info.queueFamilyIndex = queue_family_index;
			create_info.flags = create_flags;
			vk::CommandPool cmdPool;
			VK_ASSERT(logicalDevice.createCommandPool(&create_info, nullptr, &cmdPool));

			return cmdPool;
		}

		auto createBuffer(
			const vk::DeviceSize size,
			const vk::BufferUsageFlags usage_flags,
			const vk::MemoryPropertyFlags property_flags,
			vk::Buffer& buffer,
			vk::DeviceMemory &memory,
			void* data = nullptr) const -> vk::Result
		{
			vk::BufferCreateInfo buffer_info = {};
			buffer_info.size = size;
			buffer_info.usage = usage_flags;
			buffer_info.sharingMode = vk::SharingMode::eExclusive;
			VK_ASSERT(logicalDevice.createBuffer(&buffer_info, nullptr, &buffer));

			/* Memory allocation */
			vk::MemoryRequirements memory_requirements;
			logicalDevice.getBufferMemoryRequirements(buffer, &memory_requirements);

			vk::MemoryAllocateInfo allocate_info = {};
			allocate_info.allocationSize = memory_requirements.size;
			allocate_info.memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, property_flags);
			VK_ASSERT(logicalDevice.allocateMemory(&allocate_info, nullptr, &memory));

			if (nullptr != data) {
				void* mapped;
				VK_ASSERT(logicalDevice.mapMemory(memory, 0, size, static_cast<vk::MemoryMapFlagBits>(0), &mapped));
				memcpy(mapped, data, size);

				if (!(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
					vk::MappedMemoryRange mapped_memory_range = {};
					mapped_memory_range.memory = memory;
					mapped_memory_range.offset = 0;
					mapped_memory_range.size = size;
					logicalDevice.flushMappedMemoryRanges(1, &mapped_memory_range);
				}
				logicalDevice.unmapMemory(memory);
			}

			/* Attack memory to buffer */
			//VK_ASSERT_RESULT_VALUE(logicalDevice.bindBufferMemory(buffer, memory, 0));
			logicalDevice.bindBufferMemory(buffer, memory, 0); //TODO: erorr checking

			return vk::Result::eSuccess;
		}

		auto createBuffer(
			const vk::DeviceSize size,
			const vk::BufferUsageFlags usage_flags,
			const vk::MemoryPropertyFlags property_flags,
			vkpbr::Buffer& buffer,
			void *data = nullptr) const -> void
		{
			buffer.device = logicalDevice;

			vk::BufferCreateInfo buffer_create_info = {};
			buffer_create_info.usage = usage_flags;
			buffer_create_info.size = size;
			VK_ASSERT(logicalDevice.createBuffer(&buffer_create_info, nullptr, &buffer.buffer));

			vk::MemoryRequirements memory_requirements = {};
			vk::MemoryAllocateInfo memory_allocate = {};
			logicalDevice.getBufferMemoryRequirements(buffer.buffer, &memory_requirements);

			memory_allocate.allocationSize = memory_requirements.size;
			memory_allocate.memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, property_flags);
			VK_ASSERT(logicalDevice.allocateMemory(&memory_allocate, nullptr, &buffer.memory));

			buffer.alignment = memory_requirements.alignment;
			buffer.size = memory_allocate.allocationSize;
			buffer.usageFlags = usage_flags;
			buffer.memoryPropertyFlags = property_flags;

			/* If data have been passed, map buffer and copy them */
			if (nullptr != data) {
				VK_ASSERT(buffer.map());
				memcpy(buffer.mapped, data, size);
				if (!(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
					buffer.flush();
				}
				buffer.unmap();
			}

			buffer.setupDescriptor();

			buffer.bind();
		}


		auto createLogicalDevice(
			vk::PhysicalDeviceFeatures enabled_features,
			const std::vector<const char*>& enabled_extensions,
			const vk::QueueFlags& requested_queue_types = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute) -> vk::Result
		{
			auto queue_create_infos = std::vector<vk::DeviceQueueCreateInfo>();
			const auto queue_priority = float{ 0.0 };

			// Graphics queue
			if (requested_queue_types & vk::QueueFlagBits::eGraphics) {
				queueFamilyIndices.graphicsFamily = getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				vk::DeviceQueueCreateInfo queue_info = {};
				queue_info.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
				queue_info.queueCount = 1;
				queue_info.pQueuePriorities = &queue_priority;
				queue_create_infos.push_back(queue_info);
			}

			// Compute queue
			if (requested_queue_types & vk::QueueFlagBits::eCompute) {
				queueFamilyIndices.computeFamily = getQueueFamilyIndex(vk::QueueFlagBits::eCompute);
				if (queueFamilyIndices.computeFamily.value() != queueFamilyIndices.graphicsFamily.value()) {
					vk::DeviceQueueCreateInfo queue_info = {};
					queue_info.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
					queue_info.queueCount = 1;
					queue_info.pQueuePriorities = &queue_priority;
					queue_create_infos.push_back(queue_info);
				}
			} else {
				queueFamilyIndices.computeFamily = queueFamilyIndices.graphicsFamily;
			}

			auto device_extensions = std::vector<const char*>(enabled_extensions);
			device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

			vk::DeviceCreateInfo device_create_info = {};
			device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
			device_create_info.pQueueCreateInfos = queue_create_infos.data();
			device_create_info.pEnabledFeatures = &enabled_features;

			if (!device_extensions.empty()) {
				device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
				device_create_info.ppEnabledExtensionNames = device_extensions.data();
			}

			const auto result = physicalDevice.createDevice(&device_create_info, nullptr, &logicalDevice);
			if (vk::Result::eSuccess == result) {
				commandPool = createCommandPool(queueFamilyIndices.graphicsFamily.value());
			}

			this->enabledFeatures = enabled_features;

			return result;
		}
	};
}
