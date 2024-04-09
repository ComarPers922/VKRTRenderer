#pragma once
#include <Volk/volk.h>
#include <vector>

class DescriptorSetLayout
{
public:
	DescriptorSetLayout(VkDevice& logicalDevice, uint32_t setNum);

	void AddBinding(const VkDescriptorSetLayoutBinding& binding);

	void CreateDescriptorSet();

	[[nodiscard]]
	const uint32_t& GetSetNum() const
	{
		return mSetNum;
	}
	[[nodiscard]]
	VkDescriptorSetLayout GetSetLayout() const
	{
		return mDescriptorSetLayout;
	}

	void Dispose();
private:
	VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
	VkDevice& mLogicalDevice;

	std::vector<VkDescriptorSetLayoutBinding> mBindings;

	bool mIsLayoutCreated = false;
	const uint32_t mSetNum = 0;
};

