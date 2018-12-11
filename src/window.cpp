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


auto Window::Deleter::operator()(GLFWwindow* raw_handle) const -> void
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

auto Window::get() const -> GLFWwindow*
{
	return windowPointer.get();
}

auto Window::setUserPointer(void * userData) const -> void
{
	glfwSetWindowUserPointer(windowPointer.get(), userData);
}

auto Window::setKeyCallback(keyFunction function) const -> void
{
	glfwSetKeyCallback(windowPointer.get(), function);
}

auto Window::setResizeCallback(resizeFunction function) const -> void
{
	glfwSetWindowSizeCallback(windowPointer.get(), function);
}

auto Window::setMouseCallback(mouseFunction function) const -> void
{
	glfwSetCursorPosCallback(windowPointer.get(), function);
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
	VK_ASSERT_C(glfwCreateWindowSurface(static_cast<VkInstance>(instance), windowPointer.get(), allocCallback, &surface));
}

auto Window::pollEvents() -> void
{
	glfwPollEvents();
}

auto Window::getRequiredExtensions() -> std::vector<const char*>
{
	uint32_t glfw_extension_count = 0;
	const auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	return std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extension_count);
}

using window::glfw::Hint;

Hint::Hint(const int setting, const int value)
{
	this->value = { setting, value };
}