#include "Sceneloader.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

SceneLoader::SceneLoader()
{
}

SceneLoader::~SceneLoader()
{
}

void SceneLoader::init(Device* device, TextureManager* textureManager, BufferManager* bufferManager, ObjectLoader* objectLoader)
{
	this->device = device;
	this->textureManager = textureManager;
	this->bufferManager = bufferManager;
	this->objectLoader = objectLoader;
}

bool SceneLoader::loadScene(const std::string& filepath)
{
	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "Failed to open scene file: " << filepath << std::endl;
		return false;
	}

	try {
		nlohmann::json j;
		file >> j;

		parseConfig(j);
		parseCamera(j);
		parseLights(j);
		parseObjects(j);

		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Error parsing scene file: " << e.what() << std::endl;
		return false;
	}
}

void SceneLoader::cleanup()
{
	objects.clear();
	lights.clear();
}

bool SceneLoader::saveScene(const std::string& filepath) const
{
	return false;
}

void SceneLoader::parseConfig(const nlohmann::json& j)
{
	config.sceneName = j.value("sceneName", "Untitled Scene");
	config.skyboxPath = j.value("skyboxPath", "");
	config.ambientStrenght = j.value("ambientStrenght", 0.1f);
}
void SceneLoader::parseCamera(const nlohmann::json& j)
{
	if (j.contains("cameraSpawnPos") && j["cameraSpawnPos"].is_array() && j["cameraSpawnPos"].size() == 3) {
		cameraSpawnPos.x = j["cameraSpawnPos"][0].get<float>();
		cameraSpawnPos.y = j["cameraSpawnPos"][1].get<float>();
		cameraSpawnPos.z = j["cameraSpawnPos"][2].get<float>();
		bHasCamera = true;
	}
}
void SceneLoader::parseLights(const nlohmann::json& j)
{
	if (j.contains("Lights") && j["Lights"].is_array()) {
		auto lightsArray = j["Lights"];
		for (const auto& lightJson : lightsArray) {
			Light light;
			const std::string typeStr = lightJson.value("type", lightJson.value("name", "Point"));
			if (typeStr == "Directional" || typeStr == "directional") {
				light.type = LightType::Directional;
			}
			else if (typeStr == "Spot" || typeStr == "spot") {
				light.type = LightType::Spot;
			}
			else {
				light.type = LightType::Point;
			}
			
			if (lightJson.contains("position") && lightJson["position"].is_array() && lightJson["position"].size() == 3) {
				light.position.x = lightJson["position"][0].get<float>();
				light.position.y = lightJson["position"][1].get<float>();
				light.position.z = lightJson["position"][2].get<float>();
			}
			
			if (lightJson.contains("direction") && lightJson["direction"].is_array() && lightJson["direction"].size() == 3) {
				light.direction.x = lightJson["direction"][0].get<float>();
				light.direction.y = lightJson["direction"][1].get<float>();
				light.direction.z = lightJson["direction"][2].get<float>();
			}
			
			lights.push_back(light);
		}
	}
}
void SceneLoader::parseObjects(const nlohmann::json& j)
{
	if (j.contains("Objects") && j["Objects"].is_array()) {
		auto objectsArray = j["Objects"];
		for (const auto& objJson : objectsArray) {
			SceneObject obj;
			obj.name = objJson.value("name", "Unnamed Object");
			obj.modelPath = objJson.value("modelPath", "");
			
			if (objJson.contains("position") && objJson["position"].is_array() && objJson["position"].size() == 3) {
				obj.modelTransform.position.x = objJson["position"][0].get<float>();
				obj.modelTransform.position.y = objJson["position"][1].get<float>();
				obj.modelTransform.position.z = objJson["position"][2].get<float>();
			}
			
			if (objJson.contains("rotation") && objJson["rotation"].is_array() && objJson["rotation"].size() == 3) {
				obj.modelTransform.rotation.x = objJson["rotation"][0].get<float>();
				obj.modelTransform.rotation.y = objJson["rotation"][1].get<float>();
				obj.modelTransform.rotation.z = objJson["rotation"][2].get<float>();
			}
			
			if (objJson.contains("scale") && objJson["scale"].is_array() && objJson["scale"].size() == 3) {
				obj.modelTransform.scale.x = objJson["scale"][0].get<float>();
				obj.modelTransform.scale.y = objJson["scale"][1].get<float>();
				obj.modelTransform.scale.z = objJson["scale"][2].get<float>();
			}
			
			objects.push_back(obj);
		}
	}
}