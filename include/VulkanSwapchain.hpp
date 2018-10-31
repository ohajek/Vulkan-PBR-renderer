#pragma once

#include <vulkan/vulkan.hpp>
#include <VulkanDevice.hpp>
#include <Window.hpp>

using window::glfw::Window;
using namespace vkpbr;

namespace vkpbr {

	using SwapchainBuffer = struct {
		vk::Image image;
		vk::ImageView view;
	} ;

	class VulkanSwapchain {
	public:
		vk::Format                   colorFormat;
		vk::Extent2D                 extent;
		vk::ColorSpaceKHR            colorSpace;
		vk::SwapchainKHR             swapchain = nullptr;
		uint32_t                     imageCount;
		std::vector<vk::Image>       images;
		std::vector<SwapchainBuffer> buffers;
		uint32_t                     queueIndex = -1;

		VulkanSwapchain() = default;
		~VulkanSwapchain() = default;

		auto initSurface(const Window& window) -> void;
		auto attach(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, vkpbr::VulkanDevice::QueueFamilyIndices familyIndices) -> void;
		auto create(uint32_t& width, uint32_t& height) -> void;
		auto acquireNextImage(vk::Semaphore imageAvailableSemaphore, uint32_t* imageIndex) const -> vk::Result;
		auto presentToQueue(vk::Queue queue, uint32_t imageIndex, vk::Semaphore waitSemaphore) const -> vk::Result;
		auto destroy() -> void;

	private:
		vk::Instance                            instance;
		vk::Device                              device;
		vkpbr::VulkanDevice::QueueFamilyIndices familyIndices;
		vk::PhysicalDevice                      physicalDevice;
		vk::SurfaceKHR                          surface;


		/*
		Struct to hold information need to create swapchain
		capabilities : basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
		surface formats (pixel format, color space)
		available presentation modes
		*/
		struct SwapchainSupportDetails
		{
			vk::SurfaceCapabilitiesKHR        capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR>   presentModes;
		} swapchainDetails;

		auto querySwapchainSupport(vk::PhysicalDevice device) -> void;
		auto chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) const -> vk::SurfaceFormatKHR;
		auto chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const -> vk::PresentModeKHR;
		auto chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const -> vk::Extent2D;
	};
}