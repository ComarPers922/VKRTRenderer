#include "Common.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <glm/ext/quaternion_trigonometric.hpp>
#include <stb/stb_image.h>

#include "Buffer.h"

VkDeviceOrHostAddressKHR GetBufferDeviceAddress(VkDevice& logicalDevice, Buffer& buffer)
{
    VkBufferDeviceAddressInfoKHR info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr,
        buffer.GetVkBuffer(),
    };

    VkDeviceOrHostAddressKHR result;
    result.deviceAddress = vkGetBufferDeviceAddressKHR(logicalDevice, &info);

    return result;
}

VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst(VkDevice& logicalDevice, Buffer& buffer)
{
    VkDeviceOrHostAddressKHR address = GetBufferDeviceAddress(logicalDevice, buffer);

    VkDeviceOrHostAddressConstKHR result;
    result.deviceAddress = address.deviceAddress;

    return result;
}

quat QAngleAxis(const float angleRad, const vec3& axis)
{
	return glm::angleAxis(angleRad, axis);
}

vec3 QRotate(const quat& q, const vec3& v)
{
	return glm::rotate(q, v);
}

void ImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageSubresourceRange& subresourceRange,
	VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
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

VkResult DoOneTimeCommand(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
	std::function<VkResult(VkCommandBuffer&)> recordCommandFunc)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    RETURN_IF_NOT_SUCCESS(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    RETURN_IF_NOT_SUCCESS(recordCommandFunc(commandBuffer));

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

    vkDeviceWaitIdle(logicalDevice);
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

uint32_t AlignUp(const uint32_t value, const uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}

float Deg2Rad(const float& deg)
{
    return deg * (glm::pi<float>() / 180.0f);
}