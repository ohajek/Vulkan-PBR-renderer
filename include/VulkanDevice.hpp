#pragma once

#include <cassert>
#include <vector>
#include <set>
#include <optional>

#include <vulkan/vulkan.hpp>

#include <Utility.hpp>
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
			std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> computeFamily;

			auto isComplete() const -> bool
			{
				return graphicsFamily.has_value()
					&& presentFamily.has_value()
					&& computeFamily.has_value();
			}
		} queueFamilyIndices;

		operator vk::Device()
		{
			return logicalDevice;
		}

		VulkanDevice() {};

		VulkanDevice(vk::PhysicalDevice device)
			: physicalDevice(device)
		{
			physicalDevice.getProperties(&deviceProperties);
			physicalDevice.getFeatures(&deviceFeatures);
			physicalDevice.getMemoryProperties(&deviceMemoryProperties);

			/* Get queue family properties data */
			uint32_t queue_family_count = 0;
			physicalDevice.getQueueFamilyProperties(&queue_family_count, nullptr);
			assert(queue_family_count > 0);
			queueFamilyProperties.resize(queue_family_count);
			physicalDevice.getQueueFamilyProperties(&queue_family_count, queueFamilyProperties.data());

			/* List of supported extensions */
			uint32_t extension_count = 0;
			physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr);
			if (extension_count > 0) {
				auto extensions = std::vector<vk::ExtensionProperties>(extension_count);
				if (vk::Result::eSuccess == physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extension_count, &extensions.front())) {
					for (auto& extension : extensions) {
						supportedExtensions.push_back(extension.extensionName);
					}
				}
			}
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

		auto findQueueFamilies() -> void
		{
			uint32_t queueFamilyCount = 0;
			physicalDevice.getQueueFamilyProperties(&queueFamilyCount, nullptr);

			auto queue_families = std::vector<vk::QueueFamilyProperties>(queueFamilyCount);
			physicalDevice.getQueueFamilyProperties(&queueFamilyCount, queue_families.data());
			
			auto i = int{ 0 };
			for (const auto& family: queue_families) {
				if (family.queueCount > 0 && family.queueFlags & vk::QueueFlagBits::eGraphics) {
					queueFamilyIndices.graphicsFamily = i;
					queueFamilyIndices.presentFamily = i;
				}

				if (family.queueCount > 0 && family.queueFlags & vk::QueueFlagBits::eCompute) {
					queueFamilyIndices.computeFamily = i;
				}

				/*
				VkBool32 present_support = false;
				physicalDevice.getSurfaceSupportKHR(i, surface, &present_support);
				if (family.queueCount > 0 && present_support) {
					queueFamilyIndices.presentFamily = i;
				}
				*/

				if (queueFamilyIndices.isComplete()) {
					break;
				}

				i++;
			}
		}

		auto findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const -> uint32_t {
			/*
			If there is a memory type suitable for the buffer that also has
			all of the properties we need, then we return its index
			*/
			for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
				if ((typeFilter & (1 << i)) && (deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}
			throw VulkanDeviceException("[ERROR] Failed to find matching memory type.");
		}

		auto getQueueFamilyIndex(vk::QueueFlagBits queueFlags) -> uint32_t
		{
			//Looking for queue family index supporting compute but not graphics
			if (queueFlags == vk::QueueFlagBits::eCompute) {
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && (!(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics))) {
						return i;
						break;
					}
				}
			}

			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
				if (queueFamilyProperties[i].queueFlags & queueFlags) {
					return i;
					break;
				}
			}

			throw VulkanDeviceException("[ERROR] Could not find matching queue family index.");
		}

		auto createCommandBuffer(vk::CommandBufferLevel bufferLevel, bool beginRecord = false) const -> vk::CommandBuffer
		{
			vk::CommandBufferAllocateInfo allocInfo = {};
			allocInfo.commandPool = commandPool;
			allocInfo.level = bufferLevel;
			allocInfo.commandBufferCount = 1;

			vk::CommandBuffer cmd_buffer;
			if (vk::Result::eSuccess != logicalDevice.allocateCommandBuffers(&allocInfo, &cmd_buffer)) {
				throw VulkanDeviceException("[ERROR] Failed to create command buffer.");
			}

			//if requested, start recording
			if (beginRecord) {
				vk::CommandBufferBeginInfo beginInfo = {};
				//VK_CHECK_RESULT_VALUE(cmd_buffer.begin(beginInfo));
				cmd_buffer.begin(beginInfo); // TODO: error checking
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
			VK_CHECK_RESULT(logicalDevice.createFence(&fence_create_info, nullptr, &finished_executing_fence));

			VK_CHECK_RESULT(queue.submit(1, &submit_info, finished_executing_fence));
			VK_CHECK_RESULT(logicalDevice.waitForFences(1, &finished_executing_fence, true, 100000000000));
			logicalDevice.destroyFence(finished_executing_fence, nullptr);

			if (should_free_buffer) {
				logicalDevice.freeCommandBuffers(commandPool, 1, &cmd_buffer);
			}
		}

		auto createCommandPool(
			uint32_t queueFamilyIndex,
			const vk::CommandPoolCreateFlags& createFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const -> VkCommandPool
		{
			vk::CommandPoolCreateInfo create_info = {};
			create_info.queueFamilyIndex = queueFamilyIndex;
			create_info.flags = createFlags;
			vk::CommandPool cmdPool;
			VK_CHECK_RESULT(logicalDevice.createCommandPool(&create_info, nullptr, &cmdPool));

			return cmdPool;
		}

		auto createBuffer(
			vk::DeviceSize size,
			vk::BufferUsageFlags usageFlags,
			vk::MemoryPropertyFlags propertyFlags,
			vk::Buffer& buffer,
			vk::DeviceMemory &memory,
			void* data = nullptr) -> vk::Result
		{
			vk::BufferCreateInfo buffer_info = {};
			buffer_info.size = size;
			buffer_info.usage = usageFlags;
			buffer_info.sharingMode = vk::SharingMode::eExclusive;
			VK_CHECK_RESULT(logicalDevice.createBuffer(&buffer_info, nullptr, &buffer));

			/* Memory allocation */
			vk::MemoryRequirements memory_requirements;
			logicalDevice.getBufferMemoryRequirements(buffer, &memory_requirements);

			vk::MemoryAllocateInfo allocate_info = {};
			allocate_info.allocationSize = memory_requirements.size;
			allocate_info.memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, propertyFlags);
			VK_CHECK_RESULT(logicalDevice.allocateMemory(&allocate_info, nullptr, &memory));

			if (nullptr != data) {
				void* mapped;
				VK_CHECK_RESULT(logicalDevice.mapMemory(memory, 0, size, static_cast<vk::MemoryMapFlagBits>(0), &mapped));
				memcpy(mapped, data, size);

				if (!(propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
					vk::MappedMemoryRange mapped_memory_range = {};
					mapped_memory_range.memory = memory;
					mapped_memory_range.offset = 0;
					mapped_memory_range.size = size;
					logicalDevice.flushMappedMemoryRanges(1, &mapped_memory_range);
				}
				logicalDevice.unmapMemory(memory);
			}

			/* Attack memory to buffer */
			//VK_CHECK_RESULT_VALUE(logicalDevice.bindBufferMemory(buffer, memory, 0));
			logicalDevice.bindBufferMemory(buffer, memory, 0); //TODO: erorr checking

			return vk::Result::eSuccess;
		}

		auto createLogicalDevice(
			vk::PhysicalDeviceFeatures enabledFeatures,
			const std::vector<const char*>& enabledExtensions,
			const vk::QueueFlags& requestedQueueTypes = vk::QueueFlagBits::eGraphics| vk::QueueFlagBits::eCompute) -> vk::Result
		{
			auto queue_create_infos = std::vector<vk::DeviceQueueCreateInfo>();
			const auto queue_priority = float{ 0.0 };

			// Graphics queue
			findQueueFamilies();
			if (requestedQueueTypes & vk::QueueFlagBits::eGraphics) {
				//queueFamilyIndices.graphicsFamily = getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				vk::DeviceQueueCreateInfo queue_info = {};
				queue_info.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
				queue_info.queueCount = 1;
				queue_info.pQueuePriorities = &queue_priority;
				queue_create_infos.push_back(queue_info);
			}
			// Compute queue
			if (requestedQueueTypes & vk::QueueFlagBits::eCompute) {
				//queueFamilyIndices.computeFamily = getQueueFamilyIndex(vk::QueueFlagBits::eCompute);
				if (queueFamilyIndices.computeFamily != queueFamilyIndices.graphicsFamily) {
					vk::DeviceQueueCreateInfo queue_info = {};
					queue_info.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
					queue_info.queueCount = 1;
					queue_info.pQueuePriorities = &queue_priority;
					queue_create_infos.push_back(queue_info);
				}
			} else {
				queueFamilyIndices.computeFamily = queueFamilyIndices.graphicsFamily;
			}

			auto device_extensions = std::vector<const char*>(enabledExtensions);
			device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

			vk::DeviceCreateInfo device_create_info = {};
			device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
			device_create_info.pQueueCreateInfos = queue_create_infos.data();
			device_create_info.pEnabledFeatures = &enabledFeatures;

			if (!device_extensions.empty()) {
				device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
				device_create_info.ppEnabledExtensionNames = device_extensions.data();
			}

			const vk::Result result = physicalDevice.createDevice(&device_create_info, nullptr, &logicalDevice);
			if (vk::Result::eSuccess == result) {
				commandPool = createCommandPool(queueFamilyIndices.graphicsFamily.value());
			}

			this->enabledFeatures = enabledFeatures;

			return result;
		}
	};
}
