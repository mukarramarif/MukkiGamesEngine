#include <vulkan/vulkan.h>

class ComputePipeline {
	public:
	ComputePipeline();
	~ComputePipeline();
	void createComputePipeline(Device* device, const std::string& computeShaderPath);
	void cleanup(Device* device);
private:
	VkPipeline computePipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSets computeDescriptorSets;

}