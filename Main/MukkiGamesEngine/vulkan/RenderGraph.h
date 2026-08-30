#pragma once
#include "Core/VkDevice.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Frame graph ("frame-graph-lite", Vulkan Cookbook style)
//
// A pass declares which resources it reads and writes, and the graph:
//   * orders passes from those declarations (topological sort),
//   * inserts vkCmdPipelineBarrier transitions between passes,
//   * allocates transient images/buffers and reuses them across rebuilds.
//
// Contract for pass callbacks:
//   * Render passes must be created with initialLayout == finalLayout equal
//     to the layout declared in the pass usage. The graph owns transitions
//     at pass boundaries; passes must NOT insert their own barriers for
//     resources they declare.
//   * Passes are executed on the command buffer passed to the callback.
// ---------------------------------------------------------------------------

using FrameGraphHandle = uint32_t;
constexpr FrameGraphHandle FRAME_GRAPH_INVALID = ~0u;

struct FrameGraphResourceHandle {
    FrameGraphHandle handle = FRAME_GRAPH_INVALID;
};
struct FrameGraphNodeHandle {
    FrameGraphHandle handle = FRAME_GRAPH_INVALID;
};

enum FrameGraphResourceType : int32_t {
    FRAME_GRAPH_RESOURCE_TYPE_INVALID    = -1,

    FRAME_GRAPH_RESOURCE_TYPE_BUFFER     = 0,
    FRAME_GRAPH_RESOURCE_TYPE_TEXTURE    = 1,
    FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT = 2,  // texture used as a render target
    FRAME_GRAPH_RESOURCE_TYPE_REFERENCE  = 3,  // alias of another resource
};

// Creation info for transient (graph-owned) resources; imported resources
// also carry this so the graph knows format/size/usage for barriers.
struct FrameGraphResourceInfo {
    bool external = false;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // imported images only
    union {
        struct {
            VkDeviceSize      size;
            VkBufferUsageFlags usage;
        } buffer;
        struct {
            uint32_t width;
            uint32_t height;
            uint32_t layers    = 1;   // 6 => created as a cube-compatible image
            uint32_t mipLevels = 1;
            VkFormat format;
            VkImageUsageFlags usage;
            VkImageAspectFlags aspect;
        } texture;
    };
};

// How one pass touches one resource (drives barrier generation)
struct FrameGraphResourceUsage {
    FrameGraphResourceHandle resource;
    VkPipelineStageFlags     stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags            access = 0;
    VkImageLayout            layout = VK_IMAGE_LAYOUT_UNDEFINED;  // textures only
};

struct FrameGraphPass {
    std::string name;
    std::vector<FrameGraphResourceUsage> reads;
    std::vector<FrameGraphResourceUsage> writes;
    std::function<void(VkCommandBuffer)> execute;
};

class RenderGraph {
public:
    RenderGraph() = default;

    void init(Device& device);
    void reset();     // release transients into the pool, clear passes/resources
    void destroy();   // destroy the pool and all graph-owned objects

    FrameGraphResourceHandle createTexture(const FrameGraphResourceInfo& info);
    FrameGraphResourceHandle createBuffer(const FrameGraphResourceInfo& info);
    FrameGraphResourceHandle createAlias(FrameGraphResourceHandle target);

    FrameGraphResourceHandle importTexture(const FrameGraphResourceInfo& info,
                                           VkImage image,
                                           VkDeviceMemory memory,
                                           VkImageView view);
    FrameGraphResourceHandle importBuffer(const FrameGraphResourceInfo& info,
                                          VkBuffer buffer,
                                          VkDeviceMemory memory);

    FrameGraphNodeHandle addPass(std::string name,
                                 std::vector<FrameGraphResourceUsage> reads,
                                 std::vector<FrameGraphResourceUsage> writes,
                                 std::function<void(VkCommandBuffer)> execute);

    void compile();                              // order passes, allocate transients
    void execute(VkCommandBuffer commandBuffer); // barriers + pass callbacks

    // Accessors (passes use these for transient images/views)
    VkImageLayout getLayout(FrameGraphResourceHandle handle) const;
    VkImage       getImage(FrameGraphResourceHandle handle) const;
    VkImageView   getImageView(FrameGraphResourceHandle handle) const;
    VkBuffer      getBuffer(FrameGraphResourceHandle handle) const;

private:
    struct FrameGraphResource {
        FrameGraphResourceType type = FRAME_GRAPH_RESOURCE_TYPE_INVALID;
        FrameGraphResourceInfo info;
        VkBuffer       buffer       = VK_NULL_HANDLE;
        VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
        VkImage        image        = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory  = VK_NULL_HANDLE;
        VkImageView    imageView    = VK_NULL_HANDLE;
        bool owned = false;                          // graph-allocated transient
        FrameGraphHandle aliasTarget = FRAME_GRAPH_INVALID;

        // Barrier state tracking
        VkPipelineStageFlags currentStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkAccessFlags        currentAccess = 0;
        VkImageLayout        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct PooledImage {
        VkImage image; VkDeviceMemory memory; VkImageView view;
        uint32_t width, height, layers, mipLevels;
        VkFormat format; VkImageUsageFlags usage; VkImageAspectFlags aspect;
    };
    struct PooledBuffer {
        VkBuffer buffer; VkDeviceMemory memory;
        VkDeviceSize size; VkBufferUsageFlags usage;
    };

    Device* device = nullptr;

    std::vector<FrameGraphResource> resources;
    std::vector<FrameGraphPass>     passes;
    std::vector<FrameGraphHandle>   executionOrder;   // topo-sorted pass indices

    std::vector<PooledImage>  imagePool;
    std::vector<PooledBuffer> bufferPool;

    const FrameGraphResource& resolve(FrameGraphHandle handle) const;
    FrameGraphResource& resolveMutable(FrameGraphHandle handle);

    void insertBarrier(VkCommandBuffer commandBuffer, const FrameGraphResourceUsage& usage);
    void allocateImage(FrameGraphResource& res);
    void allocateBuffer(FrameGraphResource& res);
    void releaseTransients();
    void destroyPool();
};
