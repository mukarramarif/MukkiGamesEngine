#include <GlFW/glfw3.h>
class Window {
private:
	GLFWwindow* window;
	int width;
	int height;
	const char* title;
public:
	~Window();

	void init(int w, int h, const char* t);
	GLFWwindow* getGLFWwindow();
	const int getWidth();
	const int getHeight();

};