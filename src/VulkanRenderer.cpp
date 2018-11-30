#include <VulkanRenderer.hpp>
#include "Utility.hpp"
#include <vulkan/vulkan.h>


static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugFunction(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	auto message_prefix = std::string();
	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
		message_prefix = "[LAYER ERROR]:";
		break;
	}
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
		message_prefix = "[LAYER WARNING]:";
		break;
	}
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
		message_prefix = "[LAYER INFO]:";
		break;
	}
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
		message_prefix = "[LAYER DIAGNOSTIC]: ";
		break;
	}
	default:
		message_prefix = "[Validation layer]: ";
		break;
	}

	std::cerr << message_prefix << pCallbackData->pMessage << std::endl << std::endl;

	return VK_FALSE;
}


/* DEBUG MESSAGES */
auto VulkanRenderer::createDebugReportCallback(
	vk::Instance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* create_info,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* callback) -> VkResult
{
	const auto createDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
	if (createDebugReportCallback == nullptr) {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return createDebugReportCallback(instance, create_info, nullptr, callback);
}

auto VulkanRenderer::destroyDebugReportCallback(
	vk::Instance instance,
	VkDebugUtilsMessengerEXT  callback,
	const VkAllocationCallbacks* allocator) -> void
{
	const auto destroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
	destroyDebugReportCallback(instance, callback, allocator);
}




using vkpbr::VulkanRenderer;

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
	swapchain.destroy();
	device.destroyDescriptorPool(descriptorPool, nullptr);
	device.freeCommandBuffers(commandPool, static_cast<uint32_t>(drawCalls.size()), drawCalls.data());
	device.destroyRenderPass(renderPass, nullptr);
	for (auto& framebuffer : framebuffers) {
		device.destroyFramebuffer(framebuffer, nullptr);
	}
	device.destroyImageView(depthStencil.view, nullptr);
	device.destroyImage(depthStencil.image, nullptr);
	device.freeMemory(depthStencil.memory, nullptr);
	device.destroyPipelineCache(pipelineCache, nullptr);
	device.destroyCommandPool(commandPool);
	device.destroySemaphore(presentCompleteSemaphore, nullptr);
	device.destroySemaphore(renderCompleteSemaphore, nullptr);
	for (auto& fence : memoryFences) {
		device.destroyFence(fence, nullptr);
	}
	vulkanDevice.reset();
	if (settings.validation) {
		destroyDebugReportCallback(instance, debugCallback, nullptr);
	}
	instance.destroy();
}

auto VulkanRenderer::initVulkan() -> void
{
	if (createInstance() != vk::Result::eSuccess)
	{
		throw VulkanRendererException("[ERROR] Could not create Vulkan instance!");
	}

	/* Validation layers callbacks */
	setupDebugCallback();

	/* Physical device selection */
	selectPhysicalDevice();

	/* Logical device creation */
	vulkanDevice = std::make_unique<vkpbr::VulkanDevice>(physicalDevice);
	vk::PhysicalDeviceFeatures enabled_features = {};
	if (deviceFeatures.samplerAnisotropy) {
		enabled_features.samplerAnisotropy = VK_TRUE;
	}
	if(vk::Result::eSuccess != vulkanDevice->createLogicalDevice(enabled_features, wantedExtensions)) {
		throw VulkanRendererException("[ERROR] Could not create logical device!");
	}
	device = vulkanDevice->logicalDevice; //TODO: ma cenu delit se o ownership?

	/* Graphics queue */
	device.getQueue(vulkanDevice->queueFamilyIndices.graphicsFamily.value(), 0, &queue);

	/* Depth format */
	selectSuitableDepthFormat();

	/* Attach swapchain */
	swapchain.attach(instance, physicalDevice, device, vulkanDevice->queueFamilyIndices); // borrow vkpbr::device
}

auto VulkanRenderer::createInstance() -> vk::Result
{
#ifdef  _DEBUG
	settings.validation = true;
#endif

	/* Checking support for validation layers before instance creation */
	if (settings.validation && !checkValidationLayerSupport()) {
		throw VulkanRendererException("[ERROR] Validation layers requested but not supported!");
	}

	vk::ApplicationInfo programInfo = {};
	programInfo.pApplicationName = programName.c_str();
	programInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	programInfo.pEngineName = "No Engine";
	programInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	programInfo.apiVersion = VK_API_VERSION_1_0;

	vk::InstanceCreateInfo createInfo = {};
	createInfo.pApplicationInfo = &programInfo;

	/* Here we get extensions from used window system (Vulkan is platform agnostic) */
	auto extensions = window.getRequiredExtensions();
	if (settings.validation) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

#ifdef _DEBUG
	std::cout << "GLFW found extensions: " << std::endl;
	for(auto& extension : extensions) {
		std::cout << extension << std::endl;
	}
#endif

	/* Adding validation layers into the instance */
	if (settings.validation) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(wantedLayers.size());
		createInfo.ppEnabledLayerNames = wantedLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	return vk::createInstance(&createInfo, nullptr, &instance);
}

auto VulkanRenderer::prepareForRender() -> void
{
	/* Swapchain init */
	initSwapchain();
	setupSwapchain();

	/* Sync. init */
	vk::SemaphoreCreateInfo semaphore_create_info = {};
	VK_ASSERT(device.createSemaphore(&semaphore_create_info, nullptr, &presentCompleteSemaphore));
	VK_ASSERT(device.createSemaphore(&semaphore_create_info, nullptr, &renderCompleteSemaphore));

	vk::FenceCreateInfo fence_create_info = {};
	fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
	memoryFences.resize(swapchain.imageCount);
	for (auto& fence : memoryFences) {
		VK_ASSERT(device.createFence(&fence_create_info, nullptr, &fence));
	}

	/* Command pool */
	vk::CommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphicsFamily.value(); //predani graphics queue
	command_pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	VK_ASSERT(device.createCommandPool(&command_pool_create_info, nullptr, &commandPool));

	/* Command buffers / draw calls */
	createCommandBuffers();

	/* Render pass */
	createRenderPass();

	/* Pipeline */
	vk::PipelineCacheCreateInfo pipeline_cache_create_info = {};
	VK_ASSERT(device.createPipelineCache(&pipeline_cache_create_info, nullptr, &pipelineCache));

	/* Framebuffer */
	createFramebuffer();
}

auto VulkanRenderer::setupCommandBuffers() -> void
{
}

auto VulkanRenderer::setupWindow() -> void
{
	using Hint = window::glfw::Hint;

	window = window::glfw::Window(
		settings.width,
		settings.width,
		windowTitle.c_str(),
		Hint(GLFW_RESIZABLE, GLFW_TRUE),
		Hint(GLFW_CLIENT_API, GLFW_NO_API)
	);

	window.setUserPointer(this);
	window.setKeyCallback(window::glfw::Window::windowKeyCallback);
	window.setResizeCallback(windowResizeCallback);
}

auto VulkanRenderer::renderLoop() -> void
{
	while (!window.shoudlClose()) {
		glfw::Window::pollEvents();

		renderFrame();
	}
	device.waitIdle();
}

auto VulkanRenderer::renderFrame() -> void
{
	const auto frame_start = std::chrono::high_resolution_clock::now();

	//handle view change

	render();
	stopwatch.frameCounter++;

	const auto frame_end = std::chrono::high_resolution_clock::now();
	const auto diff = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
	stopwatch.delta = static_cast<float>(diff) / 1000.0f;
	//update camera

	stopwatch.fpsTimer += static_cast<float>(diff);
	if(stopwatch.fpsTimer > 1000.0f) {
		stopwatch.lastFPS = static_cast<uint32_t>(static_cast<float>(stopwatch.frameCounter) * (1000.0f / stopwatch.fpsTimer));
		stopwatch.fpsTimer = 0.0f;
		stopwatch.frameCounter = 0;
	}
}

auto VulkanRenderer::prepareFrame() -> void
{
	const auto result = swapchain.acquireNextImage(presentCompleteSemaphore, &currentBuffer);
	if ((result == vk::Result::eErrorOutOfDateKHR) || (result == vk::Result::eSuboptimalKHR)) {
		recreateSwapchain();
	}
	else {
		VK_ASSERT(result);
	}
}

auto VulkanRenderer::submitFrame() const -> void
{
	VK_ASSERT(swapchain.presentToQueue(queue, currentBuffer, renderCompleteSemaphore));
}

auto VulkanRenderer::checkValidationLayerSupport() const -> bool
{
	/* First we need to get all of validation layers */
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	/* Now we check if all of given validation layers are in supported validation layers */
	for (const char* layerName : wantedLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				std::cout << "Layer: " << layerName << " found." << std::endl;
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}
	return true;
}

auto VulkanRenderer::setupDebugCallback() -> void
{
	if (settings.validation)
	{
		std::cout << "Setting up validation layers" << std::endl;
		VkDebugUtilsMessengerCreateInfoEXT  create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = vulkanDebugFunction;
		create_info.pUserData = nullptr;
		VK_ASSERT_C(
			createDebugReportCallback(
				instance,
				&create_info,
				nullptr,
				&debugCallback
			)
		);
	}
}

auto VulkanRenderer::windowResizeCallback(GLFWwindow* window, int width, int height) -> void
{
	auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	renderer->recreateSwapchain();
}

auto VulkanRenderer::recreateSwapchain() -> void
{
	if (!preparedToRender) {
		return;
	}
	preparedToRender = false;
	
	device.waitIdle();
	//new width and height
	int new_width, new_height;
	window.getSize(new_width, new_height);
	settings.width = static_cast<uint32_t>(new_width);
	settings.height = static_cast<uint32_t>(new_height);
	setupSwapchain();
	device.destroyImageView(depthStencil.view, nullptr);
	device.destroyImage(depthStencil.image, nullptr);
	device.freeMemory(depthStencil.memory, nullptr);

	for (auto& framebuffer : framebuffers) {
		device.destroyFramebuffer(framebuffer, nullptr);
	}
	createFramebuffer();
	device.freeCommandBuffers(commandPool, static_cast<uint32_t>(drawCalls.size()), drawCalls.data());
	createCommandBuffers();
	setupCommandBuffers();
	device.waitIdle();

	preparedToRender = true;
}

auto VulkanRenderer::selectPhysicalDevice() -> void
{
	uint32_t device_count = 0;
	VK_ASSERT(instance.enumeratePhysicalDevices(&device_count, nullptr));

	if (0 == device_count)
	{
		throw VulkanRendererException("[ERROR] No physical device supports Vulkan!");
	}

	auto devices = std::vector<vk::PhysicalDevice>(device_count);
	instance.enumeratePhysicalDevices(&device_count, devices.data());

	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			physicalDevice = device;
			break;
		}
	}

	if (nullptr == physicalDevice)
	{
		throw VulkanRendererException("[ERROR] Failed to find suitable Vulkan device!");
	}

#ifdef _DEBUG
	std::cout << "Selected device: " << deviceProperties.deviceName << std::endl;
#endif
}

auto VulkanRenderer::isDeviceSuitable(const vk::PhysicalDevice& device) -> bool
{
	device.getProperties(&deviceProperties);
	device.getFeatures(&deviceFeatures);
	device.getMemoryProperties(&deviceMemoryProperties);


	return deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

auto VulkanRenderer::selectSuitableDepthFormat() -> void
{
	auto depth_formats = std::vector<vk::Format>{
	vk::Format::eD32SfloatS8Uint,
	vk::Format::eD32Sfloat,
	vk::Format::eD24UnormS8Uint,
	vk::Format::eD16UnormS8Uint,
	vk::Format::eD16Unorm
	};

	auto valid_depth_format = false;
	for (auto& format : depth_formats) {
		vk::FormatProperties format_properties;
		physicalDevice.getFormatProperties(format, &format_properties);
		if (format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			depthFormat = format;
			valid_depth_format = true;
			break;
		}
	}
	assert(valid_depth_format);
}

auto VulkanRenderer::createCommandBuffers() -> void
{
	drawCalls.resize(swapchain.imageCount);
	vk::CommandBufferAllocateInfo buffer_allocate_info = {};
	buffer_allocate_info.commandPool = commandPool;
	buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
	buffer_allocate_info.commandBufferCount = static_cast<uint32_t>(drawCalls.size());
	VK_ASSERT(device.allocateCommandBuffers(&buffer_allocate_info, drawCalls.data()));
}

auto VulkanRenderer::createRenderPass() -> void
{
	// Tady se bude kdyztak resit multisampling -> vytvorit o dva attachmenty navic
	auto attachments = std::array<vk::AttachmentDescription, 2>();

	// Color attachment
	attachments[0].format = swapchain.colorFormat;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eUndefined;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = vk::SampleCountFlagBits::e1;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eUndefined;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	/*
	Subpasses
	The index of the attachment in this array is directly referenced from
	the fragment shader with the layout(location = 0) out vec4 outColor directive
	*/
	vk::AttachmentReference color_reference = {};
	color_reference.attachment = 0;
	color_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depth_reference = {};
	depth_reference.attachment = 1;
	depth_reference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::SubpassDescription subpass_description = {};
	subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass_description.colorAttachmentCount = 1;
	subpass_description.pColorAttachments = &color_reference;
	subpass_description.pDepthStencilAttachment = &depth_reference;
	subpass_description.inputAttachmentCount = 0;
	subpass_description.pInputAttachments = nullptr;
	subpass_description.preserveAttachmentCount = 0;
	subpass_description.pPreserveAttachments = nullptr;
	subpass_description.pResolveAttachments = nullptr;

	/*
	Subpass dependencies
	We could change the waitStages for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
	to ensure that the render passes don't begin until the image is available
	or this...
	*/
	auto subpass_dependencies = std::array<vk::SubpassDependency, 2>();

	subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependencies[0].dstSubpass = 0;
	subpass_dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpass_dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpass_dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	subpass_dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	subpass_dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	subpass_dependencies[1].srcSubpass = 0;
	subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpass_dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpass_dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	subpass_dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	subpass_dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	vk::RenderPassCreateInfo render_pass_create_info = {};
	render_pass_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass_description;
	render_pass_create_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
	render_pass_create_info.pDependencies = subpass_dependencies.data();
	
	VK_ASSERT(device.createRenderPass(&render_pass_create_info, nullptr, &renderPass));
}

auto VulkanRenderer::createFramebuffer() -> void
{
	vk::ImageCreateInfo image_create_info = {};
	image_create_info.pNext = nullptr;
	image_create_info.imageType = vk::ImageType::e2D;
	image_create_info.format = depthFormat;
	image_create_info.extent = vk::Extent3D( settings.width, settings.height, 1 );
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = vk::SampleCountFlagBits::e1;
	image_create_info.tiling = vk::ImageTiling::eOptimal;
	image_create_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	//image_create_info.flags = 0;

	vk::MemoryAllocateInfo memory_allocate_info = {};
	memory_allocate_info.pNext = nullptr;
	memory_allocate_info.allocationSize = 0;
	memory_allocate_info.memoryTypeIndex = 0;

	vk::ImageViewCreateInfo depth_stencil_view = {};
	depth_stencil_view.pNext = nullptr;
	depth_stencil_view.viewType = vk::ImageViewType::e2D;
	depth_stencil_view.format = depthFormat;
	//depth_stencil_view.flags = 0;
	//depth_stencil_view.subresourceRange = {};
	depth_stencil_view.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
	depth_stencil_view.subresourceRange.baseMipLevel = 0;
	depth_stencil_view.subresourceRange.levelCount = 1;
	depth_stencil_view.subresourceRange.baseArrayLayer = 0;
	depth_stencil_view.subresourceRange.layerCount = 1;

	vk::MemoryRequirements memory_requirements = {};
	VK_ASSERT(device.createImage(&image_create_info, nullptr, &depthStencil.image));
	device.getImageMemoryRequirements(depthStencil.image, &memory_requirements);
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = vulkanDevice->findMemoryType(
		memory_requirements.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	VK_ASSERT(device.allocateMemory(&memory_allocate_info, nullptr, &depthStencil.memory));
	device.bindImageMemory(depthStencil.image, depthStencil.memory, 0);

	depth_stencil_view.image = depthStencil.image;
	VK_ASSERT(device.createImageView(&depth_stencil_view, nullptr, &depthStencil.view));

	vk::ImageView attachments[4];
	attachments[1] = depthStencil.view;

	vk::FramebufferCreateInfo framebuffer_create_info = {};
	framebuffer_create_info.pNext = nullptr;
	framebuffer_create_info.renderPass = renderPass;
	framebuffer_create_info.attachmentCount = 2;
	framebuffer_create_info.pAttachments = attachments;
	framebuffer_create_info.width = settings.width;
	framebuffer_create_info.height = settings.height;
	framebuffer_create_info.layers = 1;

	/* Framebuffer for every swapchain image */
	framebuffers.resize(swapchain.imageCount);
	for (uint32_t i = 0; i < framebuffers.size(); i++) {
		attachments[0] = swapchain.buffers[i].view;
		VK_ASSERT(device.createFramebuffer(&framebuffer_create_info, nullptr, &framebuffers[i]));
	}
}

auto VulkanRenderer::initSwapchain() -> void
{
	swapchain.initSurface(window);
}

auto VulkanRenderer::setupSwapchain() -> void
{
	swapchain.create(settings.width, settings.height);
}
