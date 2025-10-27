#include <vulkan/vulkan.h>
#include "Window.h"
#include <vector>
class VulkanRenderer {
private:
    // Core Vulkan objects
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Command infrastructure
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Renderpass & Pipeline
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // Shaders
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Textures & Resources
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    // Swapchain (for presentation)
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::set<uint32_t> uniqueQueueFamilies;
    uint32_t graphicsQueueFamilyIndex;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
public:
    // Initialization
    void init(Window* window);
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Command setup
    void createCommandPool();
    void createCommandBuffers();

    // Pipeline setup
    void createRenderPass();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createGraphicsPipeline();

    // Resource management
    void createTexture(const char* filename);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    // Rendering
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    VkSurfaceKHR getSurface();
    // Cleanup
    void cleanup();

};