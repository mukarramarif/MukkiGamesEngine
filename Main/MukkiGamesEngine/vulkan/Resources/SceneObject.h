#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include "ObjectLoader.h"
struct Transform {
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);

	glm::mat4 getModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, scale);
		return model;
	}
};

struct PhysicsProperties {
	bool enabled = false;
	bool isDynamic = false;
	bool isVehicle = false;
	float mass = 1.0f;
	bool useMeshShape = false;
};

struct ShaderConfig {
	std::string vertexShader = "shader.vert.spv";
	std::string fragmentShader = "shader.frag.spv";
	bool enableAlphaBlending = false;
	bool enableAdditiveBlending = false;
};
struct SceneObject {
	std::string name;
	std::string modelPath;
	Transform modelTransform;
	ShaderConfig shaderConfig;
	PhysicsProperties physics;
	bool visible = true;
	

	//Runtime objects
	Model model;
	bool modelLoaded = false;
	VkPipeline pipeline = VK_NULL_HANDLE;
	std::vector<std::vector<VkDescriptorSet>> descriptorSets; // [materialIndex][frameIndex]

	uint32_t id = 0; // Unique identifier for the scene object
};

struct SceneConfig{
	std::string name = "Untitled Scene";
	std::string skyboxPath = "";
	glm::vec3 ambientLightColor = glm::vec3(1.0f);
	float ambientLightIntensity = 0.1f;
	glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 cameraTarget = glm::vec3(0.0f);
};

class VehiclePhysics;

struct LoadedObject {
	Model model;
	Transform transform;
	PhysicsProperties physics;
	uint32_t sceneObjectId = 0;
	bool loaded = false;

	// Jolt physics (body ID as uint32_t, 0xFFFFFFFF = invalid)
	uint32_t physicsBodyID = 0xFFFFFFFF;
	std::unique_ptr<VehiclePhysics> vehicle;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	std::vector<std::vector<VkBuffer>> materialUniformBuffers;
	std::vector<std::vector<VkDeviceMemory>> materialUniformBuffersMemory;
	std::vector<std::vector<void*>> materialUniformBuffersMapped;
	std::vector<std::vector<VkDescriptorSet>> descriptorSets; // [materialIndex][frameIndex]
};