#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
	: position(position)
	, worldUp(up)
	, yaw(yaw)
	, pitch(pitch)
	, front(glm::vec3(0.0f, 0.0f, -1.0f))
	, movementSpeed(2.5f)
	, mouseSensitivity(0.1f)
	, zoom(45.0f)
{
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getViewMatrixNoTranslation() const
{
	// Get the full view matrix and strip translation by converting to mat3 and back
	// This keeps only the rotation component
	return glm::mat4(glm::mat3(getViewMatrix()));
}

glm::mat4 Camera::getProjectionMatrix(float aspect, float near, float far) const
{
	glm::mat4 proj = glm::perspective(glm::radians(zoom), aspect, near, far);
	// Vulkan clip space has inverted Y and half Z
	proj[1][1] *= -1;
	return proj;
}

glm::mat4 Camera::getSkyboxVPMatrix(float aspect, float near, float far) const
{
	// Combined View-Projection matrix for skybox (no translation)
	glm::mat4 view = getViewMatrixNoTranslation();
	glm::mat4 proj = getProjectionMatrix(aspect, near, far);
	return proj * view;
}

void Camera::processKeyboardInput(CameraMovement direction, float deltaTime)
{
	float velocity = movementSpeed * deltaTime;

	switch (direction) {
	case FORWARD:
		position += front * velocity;
		break;
	case BACKWARD:
		position -= front * velocity;
		break;
	case LEFT:
		position -= right * velocity;
		break;
	case RIGHT:
		position += right * velocity;
		break;
	case UP:
		position += up * velocity;
		break;
	case DOWN:
		position -= up * velocity;
		break;
	}
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
	xoffset *= mouseSensitivity;
	yoffset *= mouseSensitivity;

	yaw += xoffset;
	pitch += yoffset;

	// Constrain pitch to prevent screen flip
	if (constrainPitch) {
		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;
	}

	updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset)
{
	zoom -= yoffset;
	if (zoom < 1.0f)
		zoom = 1.0f;
	if (zoom > 45.0f)
		zoom = 45.0f;
}

void Camera::updateCameraVectors()
{
	// Calculate new front vector
	glm::vec3 newFront;
	newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	newFront.y = sin(glm::radians(pitch));
	newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	front = glm::normalize(newFront);

	// Recalculate right and up vectors
	right = glm::normalize(glm::cross(front, worldUp));
	up = glm::normalize(glm::cross(right, front));
}