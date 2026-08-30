#include "RenderGraph.h"

#include <algorithm>
#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------------
// Setup / teardown
// ---------------------------------------------------------------------------

void RenderGraph::init(Device& dev)
{
    device = &dev;
}

void RenderGraph::reset()
{
    releaseTransients();
    resources.clear();
    passes.clear();
    executionOrder.clear();
}

void RenderGraph::destroy()
{
    releaseTransients();   // owned runtime objects go into the pool
    resources.clear();
    passes.clear();
    executionOrder.clear();
    destroyPool();
}

// ---------------------------------------------------------------------------
// Resource creation / import
// ---------------------------------------------------------------------------

FrameGraphResourceHandle RenderGraph::createTexture(const FrameGraphResourceInfo& info)
{
    FrameGraphResource res{};
    res.type = FRAME_GRAPH_RESOURCE_TYPE_TEXTURE;
    res.info = info;
    res.info.external = false;
    resources.push_back(res);
    return { static_cast<FrameGraphHandle>(resources.size() - 1) };
}

FrameGraphResourceHandle RenderGraph::createBuffer(const FrameGraphResourceInfo& info)
{
    FrameGraphResource res{};
    res.type = FRAME_GRAPH_RESOURCE_TYPE_BUFFER;
    res.info = info;
    res.info.external = false;
    resources.push_back(res);
    return { static_cast<FrameGraphHandle>(resources.size() - 1) };
}

FrameGraphResourceHandle RenderGraph::createAlias(FrameGraphResourceHandle target)
{
    if (target.handle >= resources.size())
        throw std::runtime_error("RenderGraph::createAlias: invalid target handle");
    FrameGraphResource res{};
    res.type = FRAME_GRAPH_RESOURCE_TYPE_REFERENCE;
    res.aliasTarget = target.handle;
    resources.push_back(res);
    return { static_cast<FrameGraphHandle>(resources.size() - 1) };
}

FrameGraphResourceHandle RenderGraph::importTexture(const FrameGraphResourceInfo& info,
                                                    VkImage image,
                                                    VkDeviceMemory memory,
                                                    VkImageView view)
{
    FrameGraphResource res{};
    res.type = FRAME_GRAPH_RESOURCE_TYPE_TEXTURE;
    res.info = info;
    res.info.external = true;
    res.image = image;
    res.imageMemory = memory;
    res.imageView = view;
    resources.push_back(res);
    return { static_cast<FrameGraphHandle>(resources.size() - 1) };
}

FrameGraphResourceHandle RenderGraph::importBuffer(const FrameGraphResourceInfo& info,
                                                   VkBuffer buffer,
                                                   VkDeviceMemory memory)
{
    FrameGraphResource res{};
    res.type = FRAME_GRAPH_RESOURCE_TYPE_BUFFER;
    res.info = info;
    res.info.external = true;
    res.buffer = buffer;
    res.bufferMemory = memory;
    resources.push_back(res);
    return { static_cast<FrameGraphHandle>(resources.size() - 1) };
}

// ---------------------------------------------------------------------------
// Passes
// ---------------------------------------------------------------------------

FrameGraphNodeHandle RenderGraph::addPass(std::string name,
                                          std::vector<FrameGraphResourceUsage> reads,
                                          std::vector<FrameGraphResourceUsage> writes,
                                          std::function<void(VkCommandBuffer)> execute)
{
    const FrameGraphHandle nodeIndex = static_cast<FrameGraphHandle>(passes.size());

    FrameGraphPass pass;
    pass.name = std::move(name);
    pass.reads = std::move(reads);
    pass.writes = std::move(writes);
    pass.execute = std::move(execute);

    for (const auto& u : pass.reads)
        if (u.resource.handle >= resources.size())
            throw std::runtime_error("RenderGraph: pass '" + pass.name +
                                     "' reads an invalid resource handle");
    for (const auto& u : pass.writes)
        if (u.resource.handle >= resources.size())
            throw std::runtime_error("RenderGraph: pass '" + pass.name +
                                     "' writes an invalid resource handle");

    passes.push_back(std::move(pass));
    return { nodeIndex };
}

// ---------------------------------------------------------------------------
// Compile
// ---------------------------------------------------------------------------

void RenderGraph::compile()
{
    if (!device)
        throw std::runtime_error("RenderGraph::compile(): init() not called");

    // 1. Build ordering edges.
    //    For every resource, an edge is added from each pass touching it to the
    //    next pass touching it (RAW / WAR / WAW all covered). Edges only ever
    //    point forward in add order, so the graph is acyclic by construction.
    std::vector<std::set<FrameGraphHandle>> adjacency(passes.size());
    std::vector<uint32_t> indegree(passes.size(), 0);

    auto passTouches = [&](FrameGraphHandle p, FrameGraphHandle r) {
        for (const auto& u : passes[p].reads)
            if (u.resource.handle == r) return true;
        for (const auto& u : passes[p].writes)
            if (u.resource.handle == r) return true;
        return false;
    };
    auto addEdge = [&](FrameGraphHandle from, FrameGraphHandle to) {
        if (from == to || adjacency[from].count(to)) return;
        adjacency[from].insert(to);
        ++indegree[to];
    };

    for (FrameGraphHandle r = 0; r < resources.size(); ++r) {
        std::vector<FrameGraphHandle> touching;
        for (FrameGraphHandle p = 0; p < passes.size(); ++p)
            if (passTouches(p, r)) touching.push_back(p);
        for (size_t i = 0; i + 1 < touching.size(); ++i)
            addEdge(touching[i], touching[i + 1]);
    }

    // 2. Kahn topological sort.
    executionOrder.clear();
    executionOrder.reserve(passes.size());
    std::queue<FrameGraphHandle> ready;
    for (FrameGraphHandle p = 0; p < passes.size(); ++p)
        if (indegree[p] == 0) ready.push(p);

    while (!ready.empty()) {
        const FrameGraphHandle p = ready.front();
        ready.pop();
        executionOrder.push_back(p);
        for (const FrameGraphHandle v : adjacency[p])
            if (--indegree[v] == 0) ready.push(v);
    }
    if (executionOrder.size() != passes.size())
        throw std::runtime_error("RenderGraph::compile(): dependency cycle detected");

    // 3. Allocate transient resources (pool first, then fresh allocations).
    for (auto& res : resources) {
        if (res.type == FRAME_GRAPH_RESOURCE_TYPE_REFERENCE) continue;
        if (res.info.external) continue;

        if (res.type == FRAME_GRAPH_RESOURCE_TYPE_TEXTURE && res.image == VK_NULL_HANDLE) {
            const auto& t = res.info.texture;
            auto it = std::find_if(imagePool.begin(), imagePool.end(),
                                   [&](const PooledImage& p) {
                                       return p.width == t.width && p.height == t.height &&
                                              p.layers == t.layers && p.mipLevels == t.mipLevels &&
                                              p.format == t.format && p.usage == t.usage &&
                                              p.aspect == t.aspect;
                                   });
            if (it != imagePool.end()) {
                res.image = it->image;
                res.imageMemory = it->memory;
                res.imageView = it->view;
                res.owned = true;
                imagePool.erase(it);
            } else {
                allocateImage(res);
            }
        } else if (res.type == FRAME_GRAPH_RESOURCE_TYPE_BUFFER && res.buffer == VK_NULL_HANDLE) {
            const auto& b = res.info.buffer;
            auto it = std::find_if(bufferPool.begin(), bufferPool.end(),
                                   [&](const PooledBuffer& p) {
                                       return p.size == b.size && p.usage == b.usage;
                                   });
            if (it != bufferPool.end()) {
                res.buffer = it->buffer;
                res.bufferMemory = it->memory;
                res.owned = true;
                bufferPool.erase(it);
            } else {
                allocateBuffer(res);
            }
        }
    }

    // 4. Reset barrier state so execute() starts from a known state.
    for (auto& res : resources) {
        res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        res.currentAccess = 0;
        res.currentLayout = res.info.external ? res.info.initialLayout
                                              : VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

void RenderGraph::execute(VkCommandBuffer commandBuffer)
{
    for (const FrameGraphHandle p : executionOrder) {
        FrameGraphPass& pass = passes[p];

        // Merge per-resource usages: a write overrides a read so each resource
        // gets exactly one barrier per pass.
        std::unordered_map<FrameGraphHandle, FrameGraphResourceUsage> usageByResource;
        for (const auto& u : pass.reads)  usageByResource[u.resource.handle] = u;
        for (const auto& u : pass.writes) usageByResource[u.resource.handle] = u;

        for (const auto& [handle, usage] : usageByResource)
            insertBarrier(commandBuffer, usage);

        pass.execute(commandBuffer);
    }
}

void RenderGraph::insertBarrier(VkCommandBuffer commandBuffer,
                                const FrameGraphResourceUsage& usage)
{
    FrameGraphResource& res = resolveMutable(usage.resource.handle);

    if (res.type == FRAME_GRAPH_RESOURCE_TYPE_BUFFER) {
        if (res.currentStage == usage.stage && res.currentAccess == usage.access)
            return;

        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = res.currentAccess;
        barrier.dstAccessMask = usage.access;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = res.buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(commandBuffer, res.currentStage, usage.stage, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
        res.currentStage = usage.stage;
        res.currentAccess = usage.access;
        return;
    }

    // Texture / attachment.
    const VkImageLayout newLayout =
        (usage.layout != VK_IMAGE_LAYOUT_UNDEFINED) ? usage.layout : res.currentLayout;

    if (res.currentStage == usage.stage &&
        res.currentAccess == usage.access &&
        res.currentLayout == newLayout)
        return;   // nothing to do

    VkImageAspectFlags aspect = res.info.texture.aspect;
    if (aspect == 0) aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = res.currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = res.image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = res.info.texture.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = res.info.texture.layers;
    barrier.srcAccessMask = res.currentAccess;
    barrier.dstAccessMask = usage.access;

    vkCmdPipelineBarrier(commandBuffer, res.currentStage, usage.stage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    res.currentStage = usage.stage;
    res.currentAccess = usage.access;
    res.currentLayout = newLayout;
}

// ---------------------------------------------------------------------------
// Transient allocation
// ---------------------------------------------------------------------------

void RenderGraph::allocateImage(FrameGraphResource& res)
{
    const auto& t = res.info.texture;
    const bool cubemap = t.layers == 6;

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = t.format;
    info.extent = { t.width, t.height, 1 };
    info.mipLevels = t.mipLevels;
    info.arrayLayers = t.layers;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = t.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (cubemap) info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device->getDevice(), &info, nullptr, &res.image) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: failed to create transient image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device->getDevice(), res.image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        device->findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &res.imageMemory) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: failed to allocate transient image memory");

    vkBindImageMemory(device->getDevice(), res.image, res.imageMemory, 0);

    VkImageAspectFlags aspect = t.aspect;
    if (aspect == 0) aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res.image;
    viewInfo.viewType = cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = t.format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = t.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = t.layers;

    if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &res.imageView) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: failed to create transient image view");

    res.owned = true;
}

void RenderGraph::allocateBuffer(FrameGraphResource& res)
{
    device->createBuffer(res.info.buffer.size, res.info.buffer.usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         res.buffer, res.bufferMemory);
    res.owned = true;
}

void RenderGraph::releaseTransients()
{
    for (auto& res : resources) {
        if (!res.owned) continue;
        if (res.type == FRAME_GRAPH_RESOURCE_TYPE_TEXTURE) {
            imagePool.push_back({ res.image, res.imageMemory, res.imageView,
                                  res.info.texture.width, res.info.texture.height,
                                  res.info.texture.layers, res.info.texture.mipLevels,
                                  res.info.texture.format, res.info.texture.usage,
                                  res.info.texture.aspect });
        } else if (res.type == FRAME_GRAPH_RESOURCE_TYPE_BUFFER) {
            bufferPool.push_back({ res.buffer, res.bufferMemory,
                                   res.info.buffer.size, res.info.buffer.usage });
        }
        res.image = VK_NULL_HANDLE;
        res.imageView = VK_NULL_HANDLE;
        res.buffer = VK_NULL_HANDLE;
        res.owned = false;
    }
}

void RenderGraph::destroyPool()
{
    for (auto& img : imagePool) {
        vkDestroyImageView(device->getDevice(), img.view, nullptr);
        vkDestroyImage(device->getDevice(), img.image, nullptr);
        vkFreeMemory(device->getDevice(), img.memory, nullptr);
    }
    imagePool.clear();
    for (auto& buf : bufferPool) {
        vkDestroyBuffer(device->getDevice(), buf.buffer, nullptr);
        vkFreeMemory(device->getDevice(), buf.memory, nullptr);
    }
    bufferPool.clear();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const RenderGraph::FrameGraphResource& RenderGraph::resolve(FrameGraphHandle handle) const
{
    if (handle >= resources.size())
        throw std::runtime_error("RenderGraph: invalid resource handle");
    const FrameGraphResource* res = &resources[handle];
    while (res->type == FRAME_GRAPH_RESOURCE_TYPE_REFERENCE) {
        res = &resources[res->aliasTarget];
    }
    return *res;
}

RenderGraph::FrameGraphResource& RenderGraph::resolveMutable(FrameGraphHandle handle)
{
    if (handle >= resources.size())
        throw std::runtime_error("RenderGraph: invalid resource handle");
    FrameGraphResource* res = &resources[handle];
    while (res->type == FRAME_GRAPH_RESOURCE_TYPE_REFERENCE) {
        res = &resources[res->aliasTarget];
    }
    return *res;
}

VkImageLayout RenderGraph::getLayout(FrameGraphResourceHandle handle) const
{
    return resolve(handle.handle).currentLayout;
}

VkImage RenderGraph::getImage(FrameGraphResourceHandle handle) const
{
    return resolve(handle.handle).image;
}

VkImageView RenderGraph::getImageView(FrameGraphResourceHandle handle) const
{
    return resolve(handle.handle).imageView;
}

VkBuffer RenderGraph::getBuffer(FrameGraphResourceHandle handle) const
{
    return resolve(handle.handle).buffer;
}
