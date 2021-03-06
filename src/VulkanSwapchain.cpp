#include <VulkanSwapchain.hpp>

auto VulkanSwapchain::attach(
	const vk::Instance instance,
	const vk::PhysicalDevice physical_device,
	const vk::Device device,
	const vkpbr::VulkanDevice::QueueFamilyIndices family_indices) -> void
{
	this->instance = instance;
	this->physicalDevice = physical_device;
	this->device = device;
	this->familyIndices = family_indices;
}

auto VulkanSwapchain::initSurface(const Window& window) -> void
{
	// Sadly there is only C binding in GLFW
	VkSurfaceKHR tmp_surface;
	window.createWindowSurface(instance, tmp_surface, nullptr);
	surface = tmp_surface;

	uint32_t queue_count;
	physicalDevice.getQueueFamilyProperties(&queue_count, nullptr);
	assert(queue_count >= 1);

	auto queue_properties = std::vector<vk::QueueFamilyProperties>(queue_count);
	physicalDevice.getQueueFamilyProperties(&queue_count, queue_properties.data());

	auto supports_present = std::vector<VkBool32>(queue_count);
	for (uint32_t i = 0; i < queue_count; i++) {
		physicalDevice.getSurfaceSupportKHR(i, surface, &supports_present[i]);
	}

	auto graphics_queue = UINT32_MAX;
	auto present_queue = UINT32_MAX;
	for (uint32_t i = 0; i < queue_count; i++) {
		if ((queue_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
			if (graphics_queue == UINT32_MAX) {
				graphics_queue = i;
			}

			if (supports_present[i] == VK_TRUE) {
				graphics_queue = i;
				present_queue = i;
				break;
			}
		}
	}

	/* Search for separate present queue if graphics q. doesnt support it */
	if (present_queue == UINT32_MAX) {
		for (uint32_t i = 0; i < queue_count; i++) {
			if (supports_present[i] == VK_TRUE) {
				present_queue = i;
				break;
			}
		}
	}

	if (graphics_queue == UINT32_MAX || present_queue == UINT32_MAX) {
		throw std::runtime_error("[EXCEPTION] Could not find graphics or present queue!");
	}

	if (graphics_queue != present_queue) {
		throw std::runtime_error("[EXCEPTION] No support for separate graphics and present queue!");
	}

	queueIndex = graphics_queue;

	uint32_t format_count;
	VK_ASSERT(physicalDevice.getSurfaceFormatsKHR(surface, &format_count, nullptr));
	assert(format_count > 0);

	auto surface_formats = std::vector<vk::SurfaceFormatKHR>(format_count);
	VK_ASSERT(physicalDevice.getSurfaceFormatsKHR(surface, &format_count, surface_formats.data()));


	if ((format_count == 1) && (surface_formats[0].format == vk::Format::eUndefined)) {
		colorFormat = vk::Format::eB8G8R8A8Unorm;
		colorSpace = surface_formats[0].colorSpace;
	}
	else {
		auto found_desired_format = false;
		for (auto&& surface_format : surface_formats) {
			if (surface_format.format == vk::Format::eB8G8R8A8Srgb) {
				colorFormat = surface_format.format;
				colorSpace = surface_format.colorSpace;
				found_desired_format = true;
				break;
			}
		}

		if (!found_desired_format) {
			colorFormat = surface_formats[0].format;
			colorSpace = surface_formats[0].colorSpace;
		}
	}
}

auto VulkanSwapchain::create(uint32_t& width, uint32_t& height) -> void
{
	const auto old_swapchain = swapchain;

	vk::SurfaceCapabilitiesKHR surface_capabilities;
	VK_ASSERT(physicalDevice.getSurfaceCapabilitiesKHR(surface, &surface_capabilities));

	uint32_t present_mode_count;
	VK_ASSERT(physicalDevice.getSurfacePresentModesKHR(surface, &present_mode_count, nullptr));
	assert(present_mode_count > 0);

	auto present_modes = std::vector<vk::PresentModeKHR>(present_mode_count);
	VK_ASSERT(physicalDevice.getSurfacePresentModesKHR(surface, &present_mode_count, present_modes.data()));

	vk::Extent2D swapchain_extent = {};
	// special value of 0xffffffff means the surface will be set by swapchain
	if (surface_capabilities.currentExtent.width == static_cast<uint32_t>(-1)) {
		swapchain_extent.width = width;
		swapchain_extent.height = height;
	}
	else {
		swapchain_extent = surface_capabilities.currentExtent;
		width = surface_capabilities.currentExtent.width;
		height = surface_capabilities.currentExtent.height;
	}


	/* Swapchain present mode */
	const auto present_mode = vk::PresentModeKHR::eFifo;

	/* Number of images */
	auto swapchain_images_count = surface_capabilities.minImageCount + 1;
	if ((surface_capabilities.maxImageCount > 0) && (swapchain_images_count > surface_capabilities.maxImageCount)) {
		swapchain_images_count = surface_capabilities.maxImageCount;
	}

	/* Surface transformation */
	vk::SurfaceTransformFlagBitsKHR pre_transform;
	if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else {
		pre_transform = surface_capabilities.currentTransform;
	}

	auto composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	auto composite_alpha_flags = std::vector<vk::CompositeAlphaFlagBitsKHR> {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit
	};

	for (auto& alpha_flag : composite_alpha_flags) {
		if (surface_capabilities.supportedCompositeAlpha & alpha_flag) {
			composite_alpha = alpha_flag;
			break;
		}
	}

	vk::SwapchainCreateInfoKHR create_info = {};
	create_info.pNext = nullptr;
	create_info.surface = surface;
	create_info.minImageCount = swapchain_images_count;
	create_info.imageFormat = colorFormat;
	create_info.imageColorSpace = colorSpace;
	create_info.imageExtent = vk::Extent2D { swapchain_extent.width, swapchain_extent.height };
	create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	create_info.preTransform = pre_transform;
	create_info.imageArrayLayers = 1;
	create_info.imageSharingMode = vk::SharingMode::eExclusive;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = nullptr;
	create_info.presentMode = present_mode;
	create_info.oldSwapchain = old_swapchain;
	create_info.clipped = true;
	create_info.compositeAlpha = composite_alpha;

	vk::FormatProperties format_properties = {};
	physicalDevice.getFormatProperties(colorFormat, &format_properties);
	if (
		(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eTransferSrcKHR) ||
		(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)) {
		create_info.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
	}

	VK_ASSERT(device.createSwapchainKHR(&create_info, nullptr, &swapchain));

	if (old_swapchain) {
		for (uint32_t i = 0; i < imageCount; i++) {
			device.destroyImageView(buffers[i].view, nullptr);
		}
		device.destroySwapchainKHR(old_swapchain, nullptr);
	}
	VK_ASSERT(device.getSwapchainImagesKHR(swapchain, &imageCount, nullptr));

	images.resize(imageCount);
	VK_ASSERT(device.getSwapchainImagesKHR(swapchain, &imageCount, images.data()));

	buffers.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++) {
		vk::ImageViewCreateInfo color_attachment_view = {};
		color_attachment_view.pNext = nullptr;
		color_attachment_view.format = colorFormat;
		color_attachment_view.components = {
			vk::ComponentSwizzle::eR,
			vk::ComponentSwizzle::eG,
			vk::ComponentSwizzle::eB,
			vk::ComponentSwizzle::eA
		};
		color_attachment_view.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		color_attachment_view.subresourceRange.baseMipLevel = 0;
		color_attachment_view.subresourceRange.levelCount = 1;
		color_attachment_view.subresourceRange.baseArrayLayer = 0;
		color_attachment_view.subresourceRange.layerCount = 1;
		color_attachment_view.viewType = vk::ImageViewType::e2D;
		color_attachment_view.flags =  static_cast<vk::ImageViewCreateFlagBits>(0);

		buffers[i].image = images[i];
		color_attachment_view.image = buffers[i].image;

		VK_ASSERT(device.createImageView(&color_attachment_view, nullptr, &buffers[i].view));
	}
}
	
auto VulkanSwapchain::acquireNextImage(const vk::Semaphore image_available_semaphore, uint32_t* image_index) const -> vk::Result
{
	return device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphore, nullptr, image_index);
}

auto VulkanSwapchain::presentToQueue(vk::Queue queue, uint32_t image_index, vk::Semaphore wait_semaphore) const -> vk::Result
{
	vk::PresentInfoKHR present_info = {};
	present_info.pNext = nullptr;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &image_index;
	if (nullptr != wait_semaphore) {
		present_info.pWaitSemaphores = &wait_semaphore;
		present_info.waitSemaphoreCount = 1;
	}
	return queue.presentKHR(&present_info);
}

auto VulkanSwapchain::destroy() -> void
{
	if (nullptr != swapchain) {
		for (auto& buffer : buffers) {
			device.destroyImageView(buffer.view, nullptr);
		}
	}
	if (nullptr != surface) {
		device.destroySwapchainKHR(swapchain, nullptr);
		instance.destroySurfaceKHR(surface, nullptr);
	}

	surface = nullptr;
	swapchain = nullptr;
}
