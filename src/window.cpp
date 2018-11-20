#include <Window.hpp>
#include <Utility.hpp>

using window::glfw::Window;

/* Static methods */

auto Window::windowKeyCallback(GLFWwindow *pWindow, int key, int scancode, int action, int mods) -> void {
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(pWindow, true);
		break;
	}
}


auto Window::Deleter::operator()(GLFWwindow* raw_handle) -> void
{
	glfwDestroyWindow(raw_handle);
	glfwTerminate();
}

auto Window::maker(int width, int height, const char* name) -> Window::windowPtr
{
    return {
        glfwCreateWindow(width, height, name, nullptr, nullptr),
        Deleter()
    };
}

auto Window::get() -> GLFWwindow*
{
	return windowPointer.get();
}

auto Window::setUserPointer(void * userData) -> void
{
	glfwSetWindowUserPointer(windowPointer.get(), userData);
}

auto Window::setKeyCallback(keyFunction function) -> void
{
	glfwSetKeyCallback(windowPointer.get(), function);
}

auto Window::setResizeCallback(resizeFunction function) -> void
{
	glfwSetWindowSizeCallback(windowPointer.get(), function);
}

auto Window::shoudlClose() const -> bool
{
	return glfwWindowShouldClose(windowPointer.get());
}

auto Window::getSize(int& width, int& height) const -> void
{
	glfwGetWindowSize(windowPointer.get(), &width, &height);
}

auto Window::createWindowSurface(vk::Instance& instance, VkSurfaceKHR& surface, const VkAllocationCallbacks* allocCallback = nullptr) const -> void
{
	VK_CHECK_RESULT_C(glfwCreateWindowSurface(static_cast<VkInstance>(instance), windowPointer.get(), allocCallback, &surface));
}

auto Window::pollEvents() -> void
{
	glfwPollEvents();
}

auto Window::getRequiredExtensions() -> std::vector<const char*>
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

using window::glfw::Hint;

Hint::Hint(const int setting, const int value)
{
	this->value = { setting, value };
}