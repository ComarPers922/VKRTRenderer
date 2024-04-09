#include "Image.h"

#include "Common.h"
#include <string>

#include "Buffer.h"
#include "VKRTApp.h"

Image::Image(VmaAllocator& allocator, VkDevice& logicalDevice, const VkFormat& format):
	mLogicalDevice(logicalDevice),
	mAllocator(allocator),
	mFormat(format)
{

}


VkResult Image::Create(const VkImageType& imageType, const VkExtent3D& extent,
                       const VkImageTiling& tiling, const VkImageUsageFlags& usage, const VkMemoryPropertyFlags& memoryProperties)
{
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.format = mFormat;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    auto error = vmaCreateImage(mAllocator, &imageCreateInfo, &allocationCreateInfo, &mImage, &mAllocation, nullptr);
    if (error != VK_SUCCESS)
    {
        return error;
    }
}

VkResult Image::MapMemory(void** data)
{
    return vmaMapMemory(mAllocator, mAllocation, data);
}

void Image::UnmapMemory()
{
    vmaUnmapMemory(mAllocator, mAllocation);
}

void Image::UploadData(void* data, const VkDeviceSize& size)
{
    void* dst;
    MapMemory(&dst);
    memcpy(dst, data, size);
    UnmapMemory();
}

bool Image::LoadImageFromFile(const char* path,
	    const VkCommandPool& commandPool,
	    const VkQueue& graphicsQueue,
		const VkImageUsageFlags& usage, const VkMemoryPropertyFlags& memoryProperties,
		const VkImageType& imageType,
		const VkImageTiling& tiling)
{
#pragma region 首先从文件读取图片数据
    int width, height, channels;
    bool textureHDR = false;
    stbi_uc* imageData = nullptr;

    std::string fileNameString(path);
    const std::string extension = fileNameString.substr(fileNameString.length() - 3);

    if (extension == "hdr") 
    {
        textureHDR = true;
        imageData = reinterpret_cast<stbi_uc*>(stbi_loadf(path, &width, &height, &channels, STBI_rgb_alpha));
    }
    else
    {
        imageData = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    }

    if (!imageData)
    {
        imageData = stbi_load(DEFAULT_TEXTURE_DIR"error.png", &width, &height, &channels, STBI_rgb_alpha);
    }
    // 如果读取失败
    if (!imageData)
    {
        return false;
    }
#pragma endregion

    if (imageData) 
    {
        // 因为我这边的显卡的显示格式是BGR，而API读取出来的格式是RGB，所以需要交换一下顺序
        const int bpp = textureHDR ? sizeof(float[4]) : sizeof(uint8_t[4]);
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * bpp);

        for (size_t i = 0; i < width * height; i ++)
        {
            std::swap(imageData[i * 4], imageData[i * 4 + 2]);
        }
        // 申请一个临时的Staging Buffer
        Buffer stagingBuffer(mAllocator);
        VkResult error = stagingBuffer.CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        if (VK_SUCCESS == error) 
        {
            // 将图片数据上传到Buffer上，这个时候就已经可以释放图片数据了
            stagingBuffer.UploadData(imageData, imageSize);
            stbi_image_free(imageData);

            VkExtent3D imageExtent{
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            };
            // 调用刚刚写的方法创建VkImage
            auto error = Create(imageType, imageExtent, tiling, usage, memoryProperties);
            if (error != VK_SUCCESS)
            {
                return false;
            }

#pragma region 这里从CommandPool里面分配一个CommandBuffer
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            error = vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, &commandBuffer);
            if (VK_SUCCESS != error) {
                return false;
            }
#pragma endregion

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            error = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
                return false;
            }

            // 这里创建一个内存屏障，实际上这里并没有用这个来做多线程同步而是用来转换图片格式
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 我们不需要关心源格式是什么
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 首先指定图片的格式是“传输目的地”格式
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = mImage;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy region = {};
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.imageExtent = imageExtent;
            // 将刚刚创建的图片通过上面定义的Barrier拷贝到刚刚创建的VkImage那里
            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetVkBuffer(), mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // 之后将图片转换为Shader可读格式
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            // 开始转换
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            error = vkEndCommandBuffer(commandBuffer);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
                return false;
            }

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            // 启动CommandBuffer
            error = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
                return false;
            }
            // 等待程序加载图片
            error = vkQueueWaitIdle(graphicsQueue);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
                return false;
            }
            // 释放临时的Staging Buffer
            stagingBuffer.Free();
            // 释放CommandBuffer
            vkFreeCommandBuffers(mLogicalDevice, commandPool, 1, &commandBuffer);
        }
        else {
            stbi_image_free(imageData);
        }
    }
    return true;
}

VkResult Image::CreateImageView(VkImageViewType viewType, VkImageSubresourceRange subresourceRange)
{
    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = mFormat;
    imageViewCreateInfo.subresourceRange = subresourceRange;
    imageViewCreateInfo.image = mImage;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    return vkCreateImageView(mLogicalDevice, &imageViewCreateInfo, nullptr, &mImageView);
}

VkResult Image::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
	VkSamplerAddressMode addressMode)
{
    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = nullptr;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = magFilter;
    samplerCreateInfo.minFilter = minFilter;
    samplerCreateInfo.mipmapMode = mipmapMode;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = 0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    const auto result = vkCreateSampler(mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler);
    mSamplerCreated = result == VK_SUCCESS;
    return result;
}

void Image::ImageBarrier(VkCommandBuffer commandBuffer, VkImage image, const VkImageSubresourceRange& subresourceRange,
                         const VkAccessFlags& srcAccessMask, const VkAccessFlags& dstAccessMask, const VkImageLayout& oldLayout,
                         const VkImageLayout& newLayout)
{
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1,
        &imageMemoryBarrier);
}

void Image::Dispose()
{
    if (mSamplerCreated)
    {
        vkDestroySampler(mLogicalDevice, mSampler, VK_NULL_HANDLE);
        mSamplerCreated = false;
    }
    vkDestroyImageView(mLogicalDevice, mImageView, VK_NULL_HANDLE);
    vmaDestroyImage(mAllocator, mImage, mAllocation);
}
