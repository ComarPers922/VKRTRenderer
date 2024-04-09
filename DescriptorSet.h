#pragma once
#include <stdarg.h>
#include <vector>
#include <Volk/volk.h>

class DescriptorSetLayout;

class DescriptorSet
{
public:
	DescriptorSet(VkDevice& logicalDevice);

	// void InitPool(VkDescriptorPoolCreateInfo& createInfo);
	void InitPool(std::vector<DescriptorSetLayout*>&, std::vector<VkDescriptorPoolSize>&,
		const uint32_t& numMeshes, const uint32_t& numMaterials,
		const std::initializer_list<uint32_t> list);

	[[nodiscard]]
	VkDescriptorPool GetDescriptorPool() const
	{
		return mDescriptorPool;
	}

	[[nodiscard]]
	std::vector<VkDescriptorSet>& GetDescriptorSets()
	{
		return mDescriptorSets;
	}

	void Dispose();
private:
	void InitSets();

	VkDescriptorPool mDescriptorPool;
	VkDevice& mLogicalDevice;

	std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
	std::vector<VkDescriptorSet> mDescriptorSets;

	uint32_t mNumMeshes, mNumMaterials;

	std::vector<uint32_t> mDescriptorCounts;
};

