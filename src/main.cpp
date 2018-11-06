#include <memory>
#include <exception>

#include <VKPBR.hpp>



auto main() -> int
{
	try {
		auto renderer = std::make_unique<VKPBR>();
		renderer->setupWindow();
		renderer->initVulkan();
		renderer->prepareForRender();
		renderer->renderLoop();
	}
	catch (vkpbr::CustomException& e) {
		std::cerr << e.what() << std::endl;
	}
	catch (std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
    return EXIT_SUCCESS;
}