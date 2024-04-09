#pragma once
#include "Common.h"

class Buffer
{
public:
	Buffer(VmaAllocator& allocator);

	VkResult CreateBuffer(const VkDeviceSize& bufferSize,
		const VkBufferUsageFlags& usageFlags,
		const VmaAllocationCreateFlags& allocationCreateFlags,
		const VmaMemoryUsage& allocationUsage = VMA_MEMORY_USAGE_AUTO);

	void UploadData(const void* data, const VkDeviceSize& size);
	void UploadData(const void* data);
	void* Map() const;
	void Unmap() const;

	VkBuffer GetVkBuffer() const
	{
		return mVkBuffer;
	}

	[[nodiscard]]
	VkDeviceSize GetSize() const
	{
		return mSize;
	}

	void Free();

private:
	VkDeviceSize mSize;

	VmaAllocator mAllocator;

	VmaAllocation mVmaAllocation;
	VkBuffer mVkBuffer;
};

