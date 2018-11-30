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
	};

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
		auto attach(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, vkpbr::VulkanDevice::QueueFamilyIndices family_indices) -> void;
		auto create(uint32_t& width, uint32_t& height) -> void;
		auto acquireNextImage(vk::Semaphore image_available_semaphore, uint32_t* image_index) const -> vk::Result;
		auto presentToQueue(vk::Queue queue, uint32_t image_index, vk::Semaphore wait_semaphore) const -> vk::Result;
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
		using SwapchainSupportDetails = struct
		{
			vk::SurfaceCapabilitiesKHR        capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR>   presentModes;
		};

		SwapchainSupportDetails swapchainDetails;
	};
}