// MukkiGamesEngine.cpp : Defines the entry point for the application.
//

#include "MukkiGamesEngine.h"
#include "Vulkan/Window.h"
using namespace std;

int main()
{
	Window window;
	while (true) {
		window.init(800, 600, "Mukki Games Engine");
	}
	
	return 0;
}
