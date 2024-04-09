#include "DescriptorSet.h"
#include "assert.h"

#include <algorithm>
#include "DescriptorSetLayout.h"
#include "shared_with_shaders.h"

DescriptorSet::DescriptorSet(VkDevice& logicalDevice):
	mLogicalDevice(logicalDevice)
{

}

void DescriptorSet::InitPool(
	std::vector<DescriptorSetLayout*>& layouts,
	std::vector<VkDescriptorPoolSize>& poolSizes,
	const uint32_t& numMeshes, const uint32_t& numMaterials,
	const std::initializer_list<uint32_t> list)
{
	mDescriptorCounts.append_range(list);

	mNumMeshes = numMeshes;
	mNumMeshes = numMaterials;

	std::sort(layouts.begin(), layouts.end(), [](const DescriptorSetLayout* item1, const DescriptorSetLayout* item2)
	{
			return item1->GetSetNum() < item2->GetSetNum();
	});

	for (auto* layout: layouts)
	{
		mDescriptorSetLayouts.push_back(layout->GetSetLayout());
	}

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = VK_NULL_HANDLE;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = layouts.size();
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	const auto error = vkCreateDescriptorPool(mLogicalDevice, &descriptorPoolCreateInfo, VK_NULL_HANDLE, &mDescriptorPool);
	assert(error == VK_SUCCESS);

	InitSets();
}

void DescriptorSet::Dispose()
{
	// vkFreeDescriptorSets(mLogicalDevice, mDescriptorPool, mDescriptorSets.size(), mDescriptorSets.data());
	vkDestroyDescriptorPool(mLogicalDevice, mDescriptorPool, VK_NULL_HANDLE);
}

void DescriptorSet::InitSets()
{
	//mDescriptorSets.resize(SWS_NUM_SETS_NO_SKYBOX);
	//mDescriptorSets.resize(SWS_NUM_SETS);
	//std::vector<uint32_t> variableDescriptorCounts({
	//	1,
	//	mNumMeshes,      // per-face material IDs for each mesh
	//	mNumMeshes,      // vertex attribs for each mesh
	//	mNumMeshes,      // faces buffer for each mesh
	//	mNumMaterials,   // textures for each material
	//	1,              // environment texture
	//	mNumMaterials, // Colors for each material
	//	mNumMaterials,
	//	});
	const auto numSets = static_cast<uint32_t>(mDescriptorCounts.size());
	mDescriptorSets.resize(numSets);

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	//variableDescriptorCountInfo.descriptorSetCount = SWS_NUM_SETS_NO_SKYBOX;
	// variableDescriptorCountInfo.descriptorSetCount = SWS_NUM_SETS;
	// variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors	variableDescriptorCountInfo.descriptorSetCount = SWS_NUM_SETS;
	variableDescriptorCountInfo.descriptorSetCount = numSets;
	variableDescriptorCountInfo.pDescriptorCounts = mDescriptorCounts.data(); // actual number of descriptors

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
	descriptorSetAllocateInfo.descriptorPool = mDescriptorPool;
	//descriptorSetAllocateInfo.descriptorSetCount = SWS_NUM_SETS_NO_SKYBOX;
	// descriptorSetAllocateInfo.descriptorSetCount = SWS_NUM_SETS;
	descriptorSetAllocateInfo.descriptorSetCount = numSets;
	descriptorSetAllocateInfo.pSetLayouts = mDescriptorSetLayouts.data();

	const auto error = vkAllocateDescriptorSets(mLogicalDevice, &descriptorSetAllocateInfo, mDescriptorSets.data());
	assert(error == VK_SUCCESS);
}
