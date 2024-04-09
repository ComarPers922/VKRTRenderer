#pragma once

#include "Common.h"

#define DEFAULT_TEX_DIR "textures\\"

class Image
{
public:
	VkResult Create(const VkImageType& imageType,
		const VkExtent3D& extent,
		const VkImageTiling& tiling,
		const VkImageUsageFlags& usage,
		const VkMemoryPropertyFlags& memoryProperties);
	Image(VmaAllocator& allocator,
		VkDevice& logicalDevice,
		const VkFormat& format = VK_FORMAT_B8G8R8A8_UNORM);

	Image& operator=(const Image& other);

	VkResult MapMemory(void** data);
	void UnmapMemory();
	void UploadData(void* data, const VkDeviceSize& size);
	bool LoadImageFromFile(const char* path,
		const VkCommandPool& commandPool,
		const VkQueue& graphicsQueue,
		const VkImageUsageFlags& usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		const VkMemoryPropertyFlags& memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		const VkImageType& imageType = VK_IMAGE_TYPE_2D,
		const VkImageTiling& tiling = VK_IMAGE_TILING_OPTIMAL);

	VkResult CreateImageView(VkImageViewType viewType, VkImageSubresourceRange subresourceRange);

	VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);

	static void ImageBarrier(VkCommandBuffer commandBuffer,
		VkImage image,
		const VkImageSubresourceRange& subresourceRange,
		const VkAccessFlags& srcAccessMask,
		const VkAccessFlags& dstAccessMask,
		const VkImageLayout& oldLayout,
		const VkImageLayout& newLayout);

	[[nodiscard]]
	VkImage GetImage() const
	{
		return mImage;
	}

	[[nodiscard]]
	VkImageView GetImageView() const
	{
		return mImageView;
	}

	[[nodiscard]]
	VkSampler GetSampler() 	const
	{
		return mSampler;
	}

	void Dispose();
private:
	VkDevice& mLogicalDevice;
	VmaAllocator& mAllocator;
	VkFormat mFormat;

	VkImage mImage;
	VkImageView mImageView;
	VkSampler mSampler;
	VmaAllocation mAllocation;

	bool mSamplerCreated = false;
};

