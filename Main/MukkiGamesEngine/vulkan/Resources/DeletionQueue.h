#pragma once

#include <vulkan/vulkan.h>
#include <functional>
#include <deque>

/// <summary>
/// A LIFO deletion queue that stores cleanup functions for Vulkan resources.
/// Resources are pushed during creation and flushed in reverse order during cleanup.
/// This ensures proper dependency ordering (last created, first destroyed).
/// </summary>
class DeletionQueue
{
public:
	DeletionQueue() = default;
	~DeletionQueue();

	// Non-copyable, non-movable
	DeletionQueue(const DeletionQueue&) = delete;
	DeletionQueue& operator=(const DeletionQueue&) = delete;
	DeletionQueue(DeletionQueue&&) = delete;
	DeletionQueue& operator=(DeletionQueue&&) = delete;

	/// Push a generic cleanup function
	void push(std::function<void()>&& deleter);

	// --- Convenience helpers for common Vulkan resource types ---

	void pushBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory = VK_NULL_HANDLE);
	void pushImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view = VK_NULL_HANDLE);
	void pushImageView(VkDevice device, VkImageView view);
	void pushSampler(VkDevice device, VkSampler sampler);
	void pushSemaphore(VkDevice device, VkSemaphore semaphore);
	void pushFence(VkDevice device, VkFence fence);
	void pushDescriptorPool(VkDevice device, VkDescriptorPool pool);
	void pushDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout layout);
	void pushPipelineLayout(VkDevice device, VkPipelineLayout layout);
	void pushPipeline(VkDevice device, VkPipeline pipeline);

	/// Push an object with a cleanup() + delete pattern.
	/// The object's cleanup() is called, then it is deleted.
	template<typename T>
	void pushObject(T* object)
	{
		if (object)
		{
			push([object]() {
				object->cleanup();
				delete object;
			});
		}
	}

	/// Push a raw pointer to be deleted (no cleanup() required).
	template<typename T>
	void pushRawDelete(T* ptr)
	{
		if (ptr)
		{
			push([ptr]() { delete ptr; });
		}
	}

	/// Execute all pending deleters in reverse order (LIFO).
	void flush();

	/// Discard all pending deleters without executing them.
	void clear();

private:
	std::deque<std::function<void()>> m_deleters;
};
