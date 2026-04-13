#pragma once
#include "SceneObject.h"
#include "../Core/VkDevice.h"
#include "ObjectLoader.h"
#include "TextureManager.h"
#include "BufferManager.h"
#include <string>
#include <vector>
#include "../objects/lights.h"
#include <glm/fwd.hpp>
#include <nlohmann/json_fwd.hpp>




class SceneLoader {
    struct SceneConfig {
        std::string sceneName;
        std::string skyboxPath;
        float ambientStrenght = 0.1f;
    };


public:
	SceneLoader();
	~SceneLoader();
 
    void init(Device* device, TextureManager* textureManager, BufferManager* bufferManager, ObjectLoader* objectLoader);

    bool loadScene(const std::string& filepath);

    
    bool saveScene(const std::string& filepath) const;

    // Getters for the VulkanApplication to consume
    const SceneConfig& getConfig() const { return config; }
    const std::vector<SceneObject>& getObjects() const { return objects; }
    const std::vector<Light>& getLights() const { return lights; }

    
    bool hasCameraSettings() const { return bHasCamera; }
    glm::vec3 getInitialCameraPosition() const { return cameraSpawnPos; }


    void cleanup();

private:
      
    Device* device = nullptr;
    TextureManager* textureManager = nullptr;
    BufferManager* bufferManager = nullptr;
    ObjectLoader* objectLoader = nullptr;

    // Scene Data
    SceneConfig config;
    std::vector<SceneObject> objects;
    std::vector<Light> lights;

    bool bHasCamera = false;
    glm::vec3 cameraSpawnPos{ 0.0f, 0.0f, 3.0f };

    // Internal parsing helpers
    void parseConfig(const nlohmann::json& j);
    void parseCamera(const nlohmann::json& j);
    void parseLights(const nlohmann::json& j);
    void parseObjects(const nlohmann::json& j);
	
};

