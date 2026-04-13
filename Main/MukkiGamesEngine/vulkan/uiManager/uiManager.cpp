#include "uiManager.h"
#include "uiThemes.h"
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

UIManager::UIManager()
	: device(VK_NULL_HANDLE)
	, imguiPool(VK_NULL_HANDLE)
	, initialized(false)
{
}

UIManager::~UIManager()
{
	if (initialized) {
		cleanup();
	}
}

void UIManager::checkVkResult(VkResult err)
{
	if (err != VK_SUCCESS) {
		throw std::runtime_error("ImGui Vulkan error: " + std::to_string(err));
	}
}

void UIManager::createDescriptorPool(const UIRenderData& renderData)
{
	// Create descriptor pool for ImGui
	VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
	poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	if (vkCreateDescriptorPool(renderData.device, &poolInfo, nullptr, &imguiPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create ImGui descriptor pool!");
	}
}

void UIManager::init(const UIRenderData& renderData, EngineWindow* window)
{
	if (initialized) {
		return;
	}

	device = renderData.device;

	// Create descriptor pool for ImGui
	createDescriptorPool(renderData);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	
	// Setup Dear ImGui style
	/*ImGui::StyleColorsDark();*/
	// Or: ImGui::StyleColorsLight();
	setUpMainTheme(ImGui::GetStyle());
	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(window->getGLFWwindow(), true);
	
	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = renderData.instance;
	initInfo.PhysicalDevice = renderData.physicalDevice;
	initInfo.Device = renderData.device;
	initInfo.QueueFamily = renderData.queueFamily;
	initInfo.Queue = renderData.queue;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = imguiPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = renderData.minImageCount;
	initInfo.ImageCount = renderData.imageCount;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.Allocator = nullptr;
	initInfo.RenderPass = renderData.renderPass;
	initInfo.CheckVkResultFn = [](VkResult err) {
		if (err != VK_SUCCESS) {
			throw std::runtime_error("ImGui Vulkan error: " + std::to_string(err));
		}
	};

	ImGui_ImplVulkan_Init(&initInfo);

	// Upload Fonts
	VkCommandBuffer commandBuffer;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = renderData.commandPool;
	allocInfo.commandBufferCount = 1;

	vkAllocateCommandBuffers(renderData.device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	ImGui_ImplVulkan_CreateFontsTexture();
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(renderData.queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(renderData.queue);

	vkFreeCommandBuffers(renderData.device, renderData.commandPool, 1, &commandBuffer);
	

	initialized = true;
}

void UIManager::newFrame()
{
	if (!initialized) return;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

}

void UIManager::render(VkCommandBuffer commandBuffer)
{
	if (!initialized) return;

	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
}

void UIManager::cleanup()
{
	if (!initialized) return;

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (imguiPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, imguiPool, nullptr);
		imguiPool = VK_NULL_HANDLE;
	}

	initialized = false;
}

void UIManager::renderDebugWindow(float fps, float deltaTime)
{
	ImGui::Begin("Debug Info");
	ImGui::Text("FPS: %.1f", fps);
	ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);
	ImGui::Separator();
	ImGui::Text("Controls:");
	ImGui::BulletText("TAB - Toggle mouse cursor");
	ImGui::BulletText("WASD - Move camera");
	ImGui::BulletText("Mouse - Look around");
	ImGui::BulletText("B - Toggle render mode");
	ImGui::BulletText("ESC - Exit");
	ImGui::End();
}

void UIManager::renderCameraInfo(const glm::vec3& position, const glm::vec3& front)
{
	ImGui::Begin("Camera Info");
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
	ImGui::Text("Front: (%.2f, %.2f, %.2f)", front.x, front.y, front.z);
	ImGui::End();
}
void UIManager::renderLightingWindow(std::vector<Light>& lights, float& ambientStrength)
{
	ImGui::Begin("Lighting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::SliderFloat("Ambient", &ambientStrength, 0.0f, 1.0f);
	ImGui::Separator();

	// Gizmo operation selector
	ImGui::Text("Gizmo Mode:");
	if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE))
		currentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE))
		currentGizmoOperation = ImGuizmo::ROTATE;

	if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL))
		currentGizmoMode = ImGuizmo::LOCAL;
	ImGui::SameLine();
	if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD))
		currentGizmoMode = ImGuizmo::WORLD;

	ImGui::Separator();

	for (size_t i = 0; i < lights.size(); i++) {
		ImGui::PushID(static_cast<int>(i));

		// Make the header selectable for gizmo control
		bool isSelected = (selectedLightIndex == static_cast<int>(i));
		std::string header = "Light " + std::to_string(i);

		if (isSelected) {
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		}

		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			// Selection button
			if (ImGui::Button(isSelected ? "Deselect Gizmo" : "Select for Gizmo")) {
				selectedLightIndex = isSelected ? -1 : static_cast<int>(i);
			}

			ImGui::Checkbox("Enabled", &lights[i].enabled);

			const char* types[] = { "Directional", "Point", "Spot" };
			int currentType = static_cast<int>(lights[i].type);
			if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
				lights[i].type = static_cast<LightType>(currentType);
			}

			ImGui::ColorEdit3("Color", &lights[i].color.x);
			ImGui::SliderFloat("Intensity", &lights[i].intensity, 0.0f, 10.0f);

			if (lights[i].type != LightType::Directional) {
				ImGui::DragFloat3("Position", &lights[i].position.x, 0.1f);
			}

			if (lights[i].type == LightType::Directional || lights[i].type == LightType::Spot) {
				ImGui::DragFloat3("Direction", &lights[i].direction.x, 0.01f, -1.0f, 1.0f);
			}

			if (lights[i].type == LightType::Point || lights[i].type == LightType::Spot) {
				if (ImGui::TreeNode("Attenuation")) {
					ImGui::SliderFloat("Constant", &lights[i].constant, 0.0f, 2.0f);
					ImGui::SliderFloat("Linear", &lights[i].linear, 0.0f, 1.0f);
					ImGui::SliderFloat("Quadratic", &lights[i].quadratic, 0.0f, 0.5f);
					ImGui::TreePop();
				}
			}

			if (lights[i].type == LightType::Spot) {
				ImGui::SliderFloat("Cutoff Angle", &lights[i].cutoffAngle, 1.0f, 90.0f);
			}
		}

		if (isSelected) {
			ImGui::PopStyleColor();
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	if (ImGui::Button("Add Point Light") && lights.size() < MAX_LIGHTS) {
		Light newLight;
		newLight.type = LightType::Point;
		newLight.position = glm::vec3(0.0f, 2.0f, 0.0f);
		lights.push_back(newLight);
	}

	ImGui::End();
}


void UIManager::renderLightGizmo(std::vector<Light>& lights, int selectedIndex,
                                  const glm::mat4& view, const glm::mat4& projection)
{
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(lights.size())) {
		return;
	}

	Light& light = lights[selectedIndex];

	// ImGuizmo setup for full viewport
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::BeginFrame();
	
	ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, io.DisplaySize.x, io.DisplaySize.y);

	// Build transformation matrix from light position/direction
	glm::mat4 lightMatrix = glm::mat4(1.0f);
	
	if (light.type == LightType::Directional) {
		// For directional lights, use direction as rotation
		// Place gizmo at origin or a fixed distance for visibility
		glm::vec3 gizmoPos = glm::vec3(0.0f, 5.0f, 0.0f);
		lightMatrix = glm::translate(glm::mat4(1.0f), gizmoPos);
		
		// Calculate rotation from direction
		glm::vec3 dir = glm::normalize(light.direction);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		if (glm::abs(glm::dot(dir, up)) > 0.99f) {
			up = glm::vec3(1.0f, 0.0f, 0.0f);
		}
		glm::vec3 right = glm::normalize(glm::cross(up, dir));
		up = glm::cross(dir, right);
		
		lightMatrix[0] = glm::vec4(right, 0.0f);
		lightMatrix[1] = glm::vec4(up, 0.0f);
		lightMatrix[2] = glm::vec4(dir, 0.0f);
		lightMatrix[3] = glm::vec4(gizmoPos, 1.0f);
	}
	else {
		// For point/spot lights, use position
		lightMatrix = glm::translate(glm::mat4(1.0f), light.position);
		
		if (light.type == LightType::Spot) {
			// Include direction as rotation for spot lights
			glm::vec3 dir = glm::normalize(light.direction);
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			if (glm::abs(glm::dot(dir, up)) > 0.99f) {
				up = glm::vec3(1.0f, 0.0f, 0.0f);
			}
			glm::vec3 right = glm::normalize(glm::cross(up, dir));
			up = glm::cross(dir, right);
			
			glm::mat4 rotation = glm::mat4(1.0f);
			rotation[0] = glm::vec4(right, 0.0f);
			rotation[1] = glm::vec4(up, 0.0f);
			rotation[2] = glm::vec4(dir, 0.0f);
			
			lightMatrix = glm::translate(glm::mat4(1.0f), light.position) * rotation;
		}
	}

	// Manipulate the gizmo
	glm::mat4 deltaMatrix;
	if (ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(projection),
			currentGizmoOperation,
			currentGizmoMode,
			glm::value_ptr(lightMatrix),
			glm::value_ptr(deltaMatrix)))
	{
		// Extract new position
		if (light.type != LightType::Directional) {
			light.position = glm::vec3(lightMatrix[3]);
		}
		
		// Extract new direction for directional/spot lights
		if (light.type == LightType::Directional || light.type == LightType::Spot) {
			light.direction = glm::normalize(glm::vec3(lightMatrix[2]));
		}
	}
}

void UIManager::renderSceneLoader(bool& loadSceneFlag, const std::vector<std::string>& scenes, int sceneNum, const std::function<void(int)>& onLoad)
{

}

void UIManager::renderModelTransformWindow(ModelTransform& transform, float deltaTime)
{
	ImGui::Begin("Model Transform", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

	// Rotation controls
	if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Indent(10.0f);
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0f);
		ImGui::SliderFloat("Pitch (X)", &transform.rotation.x, -180.0f, 180.0f, "%.1f deg");
		ImGui::Spacing();
		ImGui::SliderFloat("Yaw (Y)", &transform.rotation.y, -180.0f, 180.0f, "%.1f deg");
		ImGui::Spacing();
		ImGui::SliderFloat("Roll (Z)", &transform.rotation.z, -180.0f, 180.0f, "%.1f deg");
		ImGui::PopItemWidth();

		ImGui::Spacing();
		ImGui::Spacing();
		if (ImGui::Button("Reset Rotation", ImVec2(120, 0))) {
			transform.rotation = glm::vec3(0.0f);
		}

		ImGui::Unindent(10.0f);
		ImGui::Spacing();
	}

	ImGui::Separator();
	ImGui::Spacing();

	// Auto-rotation controls
	if (ImGui::CollapsingHeader("Auto Rotate", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Indent(10.0f);
		ImGui::Spacing();

		ImGui::Checkbox("Enable Auto Rotate", &transform.autoRotate);

		if (transform.autoRotate) {
			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::PushItemWidth(200.0f);
			ImGui::SliderFloat("Speed", &transform.autoRotateSpeed, 0.0f, 360.0f, "%.1f deg/s");
			ImGui::Spacing();

			const char* axisNames[] = { "X (Pitch)", "Y (Yaw)", "Z (Roll)" };
			ImGui::Combo("Axis", &transform.autoRotateAxis, axisNames, IM_ARRAYSIZE(axisNames));
			ImGui::PopItemWidth();

			// Apply auto rotation
			float rotationDelta = transform.autoRotateSpeed * deltaTime;
			switch (transform.autoRotateAxis) {
			case 0: transform.rotation.x += rotationDelta; break;
			case 1: transform.rotation.y += rotationDelta; break;
			case 2: transform.rotation.z += rotationDelta; break;
			}

			// Keep rotation in [-180, 180] range
			for (int i = 0; i < 3; i++) {
				while (transform.rotation[i] > 180.0f) transform.rotation[i] -= 360.0f;
				while (transform.rotation[i] < -180.0f) transform.rotation[i] += 360.0f;
			}
		}

		ImGui::Unindent(10.0f);
		ImGui::Spacing();
	}

	ImGui::Separator();
	ImGui::Spacing();

	// Position controls
	if (ImGui::CollapsingHeader("Position")) {
		ImGui::Indent(10.0f);
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0f);
		ImGui::DragFloat("X##pos", &transform.position.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::Spacing();
		ImGui::DragFloat("Y##pos", &transform.position.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::Spacing();
		ImGui::DragFloat("Z##pos", &transform.position.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::Spacing();
		ImGui::Spacing();
		if (ImGui::Button("Reset Position", ImVec2(120, 0))) {
			transform.position = glm::vec3(0.0f);
		}

		ImGui::Unindent(10.0f);
		ImGui::Spacing();
	}

	ImGui::Separator();
	ImGui::Spacing();

	// Scale control
	if (ImGui::CollapsingHeader("Scale")) {
		ImGui::Indent(10.0f);
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0f);
		ImGui::SliderFloat("Uniform Scale", &transform.scale, 0.1f, 500.0f, "%.1f");
		ImGui::PopItemWidth();

		ImGui::Spacing();
		ImGui::Spacing();
		if (ImGui::Button("Reset Scale", ImVec2(120, 0))) {
			transform.scale = 100.0f;
		}

		ImGui::Unindent(10.0f);
		ImGui::Spacing();
	}

	ImGui::PopStyleVar(2);

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Spacing();

	// Center the reset all button
	float buttonWidth = 150.0f;
	float windowWidth = ImGui::GetWindowSize().x;
	ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

	if (ImGui::Button("Reset All", ImVec2(buttonWidth, 30))) {
		transform = ModelTransform{};
	}

	ImGui::Spacing();
	ImGui::End();
}