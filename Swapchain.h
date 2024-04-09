#pragma once
#include "Common.h"

class Swapchain
{
public:
	Swapchain(VkPhysicalDevice& physicalDevice,
		VkDevice& logicalDevice,
		VkSurfaceKHR& surface,
		VkSurfaceFormatKHR& surfaceFormat,
		const uint32_t& width,
		const uint32_t& height);


	[[nodiscard]]
	const uint32_t& GetImageCount() const
	{
		return mImageCount;
	}
	[[nodiscard]]
	const VkExtent2D& GetSwapchainExtent() const
	{
		return mSwapchainExtent;
	}
	[[nodiscard]]
	VkImage GetImage(size_t index) const
	{
		return mSwapchainImages[index];
	}
	[[nodiscard]]
	VkImageView GetImageView(size_t index) const
	{
		return mSwapchainImageViews[index];
	}
	[[nodiscard]]
	VkSwapchainKHR GetSwapchain()
	{
		return mSwapchain;
	}
	[[nodiscard]]
	VkFence GetFence(size_t index)
	{
		return mSwapchainFences[index];
	}

	void Dispose();
private:
	VkDevice& mLogicalDevice;

	VkSwapchainKHR mSwapchain;
	std::vector<VkImageView> mSwapchainImageViews;
	std::vector<VkImage> mSwapchainImages;
	std::vector<VkFence> mSwapchainFences;

	uint32_t mImageCount = 0;

	VkExtent2D mSwapchainExtent;
};

