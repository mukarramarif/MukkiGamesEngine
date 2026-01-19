#pragma once

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

	Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
		   glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
		   float yaw = -90.0f,
		   float pitch = 0.0f);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getViewMatrixNoTranslation() const;  // For skybox - rotation only
	glm::mat4 getProjectionMatrix(float aspect, float near = 0.1f, float far = 100.0f) const;
	glm::mat4 getSkyboxVPMatrix(float aspect, float near = 0.1f, float far = 100.0f) const;

	void processKeyboardInput(CameraMovement direction, float deltaTime);
	void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
	void processMouseScroll(float yoffset);

private:
	void updateCameraVectors();
};