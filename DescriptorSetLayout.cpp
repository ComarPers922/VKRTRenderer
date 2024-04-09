#include "DescriptorSetLayout.h"
#include <assert.h>

DescriptorSetLayout::DescriptorSetLayout(VkDevice& logicalDevice, uint32_t setNum):
	mLogicalDevice(logicalDevice), mSetNum(setNum)
{

}

void DescriptorSetLayout::AddBinding(const VkDescriptorSetLayoutBinding& binding)
{
	if (mIsLayoutCreated)
	{
		return;
	}

	mBindings.push_back(binding);
}

void DescriptorSetLayout::CreateDescriptorSet()
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(mBindings.size()),
		.pBindings = mBindings.data(),
	};

	auto error = vkCreateDescriptorSetLayout(
		mLogicalDevice,
		&descriptorSetLayoutCreateInfo,
		nullptr,
		&mDescriptorSetLayout);
	assert(error == VK_SUCCESS);

	mIsLayoutCreated = true;
}

void DescriptorSetLayout::Dispose()
{
	vkDestroyDescriptorSetLayout(mLogicalDevice, mDescriptorSetLayout, VK_NULL_HANDLE);
}
