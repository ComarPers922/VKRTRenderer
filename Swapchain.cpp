#include "Swapchain.h"

Swapchain::Swapchain(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice,
    VkSurfaceKHR& surface, VkSurfaceFormatKHR& surfaceFormat,
	const uint32_t& width, const uint32_t& height):
mLogicalDevice(logicalDevice)
{
    // 查看当前surface支持的Swapchain的程度
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    assert(error == VK_SUCCESS);

    // 确保当前Swapchain的大小不要超出限制
    auto resolutionX = glm::clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.currentExtent.width);
    auto resolutionY = glm::clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.currentExtent.height);
    // 使用MAILBOX交换模式，方便在程序前后交换的时候总能拿到最新的那一张图片交换
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    VkSwapchainKHR prevSwapchain = mSwapchain;

    // 创建交换链的参数
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    // 注意回顾上一章的内容，这里的SurfaceFormat要与上一章获取的Surface Format一致
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = { resolutionX, resolutionY };
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = prevSwapchain;
    // 创建交换链
    error = vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, nullptr, &mSwapchain);
    assert(error == VK_SUCCESS);

    if (prevSwapchain)
    {
        for (VkImageView& imageView : mSwapchainImageViews)
        {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
        vkDestroySwapchainKHR(logicalDevice, prevSwapchain, nullptr);
    }
    // 查看当前交换链支持多少张图片，并获取这些图片，可能是双缓冲也可能是三缓冲等等
    // 创建交换链的时候会同时创建图片，可以直接拿去用作画布
    vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, nullptr);
    mSwapchainImages.resize(mImageCount);
    vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, mSwapchainImages.data());
    // 有多少图片就创建多少ImageViews，一一对应
    mSwapchainImageViews.resize(mImageCount);
    for (uint32_t i = 0; i < mImageCount; ++i)
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image = mSwapchainImages[i];
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.components = { };

        error = vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &mSwapchainImageViews[i]);
        assert(error == VK_SUCCESS);

        mSwapchainExtent = { resolutionX, resolutionY };
    }
    // 交换链有多少图片就创建多少Fences
    // Fences是程序渲染时候用来做同步用的，后面会专门讲
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    mSwapchainFences.resize(mSwapchainImages.size());
    for (auto& fence : mSwapchainFences)
    {
        error = vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &fence);
        assert(error == VK_SUCCESS);
    }
}

void Swapchain::Dispose()
{
    // 销毁ImageViews
    for (auto& view: mSwapchainImageViews)
    {
        vkDestroyImageView(mLogicalDevice, view, VK_NULL_HANDLE);
    }

    // 注意：交换链的VkImages不需要手动销毁，会报错！（这里代码被注释了只是为了展示一下，大家在实际开发的时候别这么写！）
    //for (auto& image: mSwapchainImages)
    //{
    //    vkDestroyImage(mLogicalDevice, image, VK_NULL_HANDLE);
    //}
    // 消费用于同步的Fences
    for (auto& fence: mSwapchainFences)
    {
        vkDestroyFence(mLogicalDevice, fence, VK_NULL_HANDLE);
    }
    // 消费交换链的时候其附带的VKImages就自动被销毁了！
    vkDestroySwapchainKHR(mLogicalDevice, mSwapchain, VK_NULL_HANDLE);
}
