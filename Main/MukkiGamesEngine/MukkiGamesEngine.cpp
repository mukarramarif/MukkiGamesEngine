// MukkiGamesEngine.cpp : Defines the entry point for the application.
//

#include "MukkiGamesEngine.h"
#include "Renderer/VulkanRenderer.h"


int main(int argc, char* argv[])
{
	RenderConfig config;
	std::string backend = "vulkan";

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--backend" && i + 1 < argc) {
			backend = argv[++i];
		} else if (arg == "--scene" && i + 1 < argc) {
			config.scenePath = argv[++i];
		} else if (arg == "--width" && i + 1 < argc) {
			config.windowWidgth = std::stoi(argv[++i]);
		} else if (arg == "--height" && i + 1 < argc) {
			config.windowHeight = std::stoi(argv[++i]);
		} else if (arg == "--title" && i + 1 < argc) {
			config.windowTitle = argv[++i];
		}
	}

	if (backend == "vulkan") {
		VulkanRenderer renderer;
		renderer.init(config);
		renderer.run();
		renderer.shutdown();
	} else {
		std::cout << "Unknown backend: " << backend << std::endl;
		std::cout << "Usage: ./exe --backend vulkan [--scene <path>] [--width <w>] [--height <h>] [--title <title>]" << std::endl;
	}

	return 0;
}
