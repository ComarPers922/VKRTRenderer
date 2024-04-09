#include "Buffer.h"

#include "VKRTApp.h"

Buffer::Buffer(VmaAllocator& allocator):
	mAllocator(allocator)
{

}

VkResult Buffer::CreateBuffer(const VkDeviceSize& bufferSize,
    const VkBufferUsageFlags& usageFlags, 
    const VmaAllocationCreateFlags& allocationCreateFlags,
    const VmaMemoryUsage& allocationUsage)
{
    // 指定Buffer大小
    mSize = bufferSize;

    // 创建VkBuffer的一些参数
    VkBufferCreateInfo bufferCreateInfo = { };
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = usageFlags;

    // 创建VmaAllocation的一些参数
    VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
    bufferAllocationCreateInfo.usage = allocationUsage;
    bufferAllocationCreateInfo.flags = allocationCreateFlags;

    VmaAllocationInfo bufferAllocationInfo = {};
    const auto error = vmaCreateBuffer(mAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &mVkBuffer, &mVmaAllocation, &bufferAllocationInfo);

    // 返回创建结果
    return error;
}

void Buffer::UploadData(const void* data, const VkDeviceSize& size)
{
    void* memory = this->Map();
    memcpy(memory, data, size);
    this->Unmap();
}

void Buffer::UploadData(const void* data)
{
    UploadData(data, mSize);
}

void* Buffer::Map() const
{
    void* mem = nullptr;

    auto result = vmaMapMemory(mAllocator, mVmaAllocation, &mem);
    assert(result == VK_SUCCESS);
    //if (VK_SUCCESS != result)
    //{
    //    mem = nullptr;
    //}

    return mem;
}

void Buffer::Unmap() const
{
    vmaUnmapMemory(mAllocator, mVmaAllocation);
}

void Buffer::Free()
{
    vmaDestroyBuffer(mAllocator, mVkBuffer, mVmaAllocation);
}

