#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraMovement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Camera {
public:
	// Camera attributes
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	// Euler angles
	float yaw;
	float pitch;

	// Camera options
	float movementSpeed;
	float mouseSensitivity;
	float zoom;

	// Constructor with vectors
	Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
		float yaw = -90.0f,
		float pitch = 0.0f);

	// Returns the view matrix
	glm::mat4 getViewMatrix() const;

	// Returns the projection matrix
	glm::mat4 getProjectionMatrix(float aspect, float near = 0.1f, float far = 100.0f) const;

	// Process keyboard input
	void processKeyboardInput(CameraMovement direction, float deltaTime);

	// Process mouse movement
	void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

	// Process mouse scroll
	void processMouseScroll(float yoffset);

private:
	// Update camera vectors from Euler angles
	void updateCameraVectors();
};