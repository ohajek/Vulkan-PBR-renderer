#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <utility>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


#include <CustomException.hpp>


namespace window {

	namespace glfw {

		using vkpbr::CustomException;
		class WindowGLFWException : public CustomException {
			using CustomException::CustomException;
		};


		class Window {
		public:
			Window() = default;

			template<typename ...Ts>
			Window(const int width, const int height, const char* name, Ts&&... hints)
			{
				glfwInit();
				setHints(std::forward<Ts>(hints)...);
				windowPointer = maker(width, height, name);
				if (nullptr == windowPointer.get()) {
					throw WindowGLFWException("ERROR: Failed to create GLFW window.");
				}
			} // WindowGLFW

			GLFWwindow* get() const;

			auto setUserPointer(void* userData) const -> void;

			using keyFunction = void(GLFWwindow*, int, int, int, int);
			auto setKeyCallback(keyFunction function) const -> void;

			using resizeFunction = void(GLFWwindow*, int, int);
			auto setResizeCallback(resizeFunction function) const -> void;

			auto shoudlClose() const -> bool;

			auto getSize(int& width, int& height) const -> void;

			auto Window::createWindowSurface(vk::Instance& instance, VkSurfaceKHR& surface, const VkAllocationCallbacks* allocCallback) const -> void;

			static auto pollEvents() -> void;

			static auto getRequiredExtensions() -> std::vector<const char*>;

			static auto windowKeyCallback(GLFWwindow *pWindow, int key, int scancode, int action, int mods) -> void;

			class Deleter {
			public:
				auto operator()(GLFWwindow* raw_handle) const -> void;
			};

		protected:
			using windowPtr = std::unique_ptr<GLFWwindow, Deleter>;
			windowPtr windowPointer;

			template<typename T, typename ...Ts>
			static auto setHints(T& head, Ts&&... tail) -> void
			{
				static_assert(std::is_same<Hint, T>::value, "Construct GLFW window only with GLFW hints.");
				glfwWindowHint(head.value.first, head.value.second);
				setHints(std::forward<Ts>(tail)...);
			}

			static auto setHints() -> void {};


		private:
			static auto maker(const int width, const int height, const char* name) -> windowPtr;
		};
		

		struct Hint {
		public:
			Hint(const int setting, const int value);

			std::pair<int, int> value;
		};
	} // namespace glfw
} // namespace window