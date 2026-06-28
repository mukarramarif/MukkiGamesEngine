#include "DeletionQueue.h"

DeletionQueue::~DeletionQueue()
{
	// If the user forgot to flush, flush now.
	// This is a safety net but flush() should be called explicitly.
	flush();
}

void DeletionQueue::push(std::function<void()>&& deleter)
{
	m_deleters.push_back(std::move(deleter));
}

void DeletionQueue::pushBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
	if (buffer != VK_NULL_HANDLE || memory != VK_NULL_HANDLE)
	{
		push([device, buffer, memory]() {
			if (buffer != VK_NULL_HANDLE)
				vkDestroyBuffer(device, buffer, nullptr);
			if (memory != VK_NULL_HANDLE)
				vkFreeMemory(device, memory, nullptr);
		});
	}
}

void DeletionQueue::pushImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view)
{
	if (image != VK_NULL_HANDLE || memory != VK_NULL_HANDLE || view != VK_NULL_HANDLE)
	{
		push([device, image, memory, view]() {
			if (view != VK_NULL_HANDLE)
				vkDestroyImageView(device, view, nullptr);
			if (image != VK_NULL_HANDLE)
				vkDestroyImage(device, image, nullptr);
			if (memory != VK_NULL_HANDLE)
				vkFreeMemory(device, memory, nullptr);
		});
	}
}

void DeletionQueue::pushImageView(VkDevice device, VkImageView view)
{
	if (view != VK_NULL_HANDLE)
	{
		push([device, view]() {
			vkDestroyImageView(device, view, nullptr);
		});
	}
}

void DeletionQueue::pushSampler(VkDevice device, VkSampler sampler)
{
	if (sampler != VK_NULL_HANDLE)
	{
		push([device, sampler]() {
			vkDestroySampler(device, sampler, nullptr);
		});
	}
}

void DeletionQueue::pushSemaphore(VkDevice device, VkSemaphore semaphore)
{
	if (semaphore != VK_NULL_HANDLE)
	{
		push([device, semaphore]() {
			vkDestroySemaphore(device, semaphore, nullptr);
		});
	}
}

void DeletionQueue::pushFence(VkDevice device, VkFence fence)
{
	if (fence != VK_NULL_HANDLE)
	{
		push([device, fence]() {
			vkDestroyFence(device, fence, nullptr);
		});
	}
}

void DeletionQueue::pushDescriptorPool(VkDevice device, VkDescriptorPool pool)
{
	if (pool != VK_NULL_HANDLE)
	{
		push([device, pool]() {
			vkDestroyDescriptorPool(device, pool, nullptr);
		});
	}
}

void DeletionQueue::pushDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout layout)
{
	if (layout != VK_NULL_HANDLE)
	{
		push([device, layout]() {
			vkDestroyDescriptorSetLayout(device, layout, nullptr);
		});
	}
}

void DeletionQueue::pushPipelineLayout(VkDevice device, VkPipelineLayout layout)
{
	if (layout != VK_NULL_HANDLE)
	{
		push([device, layout]() {
			vkDestroyPipelineLayout(device, layout, nullptr);
		});
	}
}

void DeletionQueue::flush()
{
	// Execute in reverse order (LIFO) so resources are destroyed
	// in the opposite order of creation, respecting Vulkan dependencies.
	while (!m_deleters.empty())
	{
		auto& deleter = m_deleters.back();
		if (deleter)
			deleter();
		m_deleters.pop_back();
	}
}

void DeletionQueue::clear()
{
	m_deleters.clear();
}
