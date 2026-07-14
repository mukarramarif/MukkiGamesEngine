// MukkiGamesEngine.cpp : Defines the entry point for the application.
//

#include "MukkiGamesEngine.h"
#include "Render"

using namespace std;

int main(int argc, std::vector<std::string> argv)
{
	if(argc > 1 && argv[1] == "vulkan") {
        auto renderer = createVulkanRenderer();
        renderer->init(RenderConfig{});
        renderer->shutdown();
    }
    else {
        std::cout << "Please specify a rendering backend (e.g., 'vulkan')." << std::endl;
    }
    
	return 0;
}
