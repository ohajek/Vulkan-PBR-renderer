#pragma once

#include <memory>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <window.hpp>
#include <VulkanDevice.hpp>
#include <VulkanSwapchain.hpp>
#include <Camera.hpp>
#include <CustomException.hpp>

using namespace vkpbr;
using namespace window;

namespace vkpbr {

	using vkpbr::CustomException;
	class VulkanRendererException : public CustomException {
		using CustomException::CustomException;
	};


	class VulkanRenderer {
	public:
		using Settings = struct {
			uint32_t 					width = 1280;
			uint32_t 					height = 720;
			bool						validation = false;
			bool						fullscreen = false;
			bool						vsync = false;
			bool						multisampling = true;
			vk::SampleCountFlagBits 	sampleCount = vk::SampleCountFlagBits::e4;
		};

		using FPScontainer = struct {
			float    fpsTimer = 0.0f;
			uint32_t frameCounter = 0;
			uint32_t lastFPS = 0;
			float    delta = 1.0f;
		};

		Settings                             settings;
		glfw::Window                         window;
		std::unique_ptr<vkpbr::VulkanDevice> vulkanDevice;
		std::string                          programName;
		std::string                          windowTitle;
		vkpbr::Camera                        camera;
		const std::vector<const char*>       wantedLayers = {
			"VK_LAYER_LUNARG_standard_validation",
			"VK_LAYER_LUNARG_assistant_layer",
			"VK_LAYER_LUNARG_core_validation"
		};
		const std::vector<const char*> wantedExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
		FPScontainer stopwatch;
		glm::vec2    mousePosition;


		VulkanRenderer();
		virtual ~VulkanRenderer();

		auto initVulkan() -> void;
		virtual auto render() -> void = 0;
		virtual auto createInstance() -> vk::Result;
		virtual auto prepareForRender() -> void;
		virtual auto setupCommandBuffers() -> void;
		virtual auto createFramebuffer() -> void;
		virtual auto updateView() -> void;
		virtual auto updateUI() -> void;

		auto setupWindow() -> void;
		auto renderLoop() -> void;
		auto renderFrame() -> void;
		auto prepareFrame() -> void;
		auto submitFrame() const -> void;

		auto initSwapchain() -> void;
		auto setupSwapchain() -> void;
	
	protected:
		vk::Instance                         instance;
		vk::PhysicalDevice                   physicalDevice;
		vk::PhysicalDeviceProperties         deviceProperties;
		vk::PhysicalDeviceFeatures           deviceFeatures;
		vk::PhysicalDeviceMemoryProperties   deviceMemoryProperties;
		vk::Device                           device;
		vk::Queue                            queue;
		vkpbr::VulkanSwapchain               swapchain;
		vk::CommandPool                      commandPool;
		vk::Semaphore                        presentCompleteSemaphore;
		vk::Semaphore                        renderCompleteSemaphore;
		std::vector<vk::Fence>               memoryFences;
		std::vector<vk::CommandBuffer>       drawCalls;
		vk::Format                           depthFormat;
		vk::RenderPass                       renderPass;
		vk::DescriptorPool                   descriptorPool;
		vk::PipelineCache                    pipelineCache;
		std::vector<vk::Framebuffer>         framebuffers;
		bool                                 swapchain_recreated;
		uint32_t                             currentBuffer = 0;
		bool                                 preparedToRender = false;
		glm::vec3                            rotation = glm::vec3();
		glm::vec3                            cameraPos = glm::vec3();

		auto checkValidationLayerSupport() const -> bool;

	private:
		using DepthStencil = struct {
			vk::Image        image;
			vk::ImageView    view;
			vk::DeviceMemory memory;
		};

		DepthStencil             depthStencil;
		bool                     viewChanged = false;
		VkDebugUtilsMessengerEXT debugCallback;

		static auto createDebugReportCallback(
			vk::Instance instance,
			const VkDebugUtilsMessengerCreateInfoEXT * create_info,
			const VkAllocationCallbacks * allocator,
			VkDebugUtilsMessengerEXT * callback
		)->VkResult;
		static auto destroyDebugReportCallback(
			vk::Instance instance,
			VkDebugUtilsMessengerEXT callback,
			const VkAllocationCallbacks * allocator
		) -> void;
		auto setupDebugCallback() -> void;

		static auto windowResizeCallback(GLFWwindow* window, int width, int height) -> void;
		static auto keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void;
		static auto mouseCallback(GLFWwindow* window, double xpos, double ypos) -> void;
		auto keyPressed(GLFWwindow *window, int key, int scancode, int action, int mods) -> void;
		auto recreateSwapchain() -> void;

		auto selectPhysicalDevice() -> void;
		auto isDeviceSuitable(const vk::PhysicalDevice& device) -> bool;

		auto selectSuitableDepthFormat() -> void;

		auto createCommandBuffers() -> void;
		auto createRenderPass() -> void;

	};
}