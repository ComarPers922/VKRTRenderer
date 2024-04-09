#include "Swapchain.h"

Swapchain::Swapchain(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice,
    VkSurfaceKHR& surface, VkSurfaceFormatKHR& surfaceFormat,
	const uint32_t& width, const uint32_t& height):
mLogicalDevice(logicalDevice)
{
    // �鿴��ǰsurface֧�ֵ�Swapchain�ĳ̶�
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    assert(error == VK_SUCCESS);

    // ȷ����ǰSwapchain�Ĵ�С��Ҫ��������
    auto resolutionX = glm::clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.currentExtent.width);
    auto resolutionY = glm::clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.currentExtent.height);
    // ʹ��MAILBOX����ģʽ�������ڳ���ǰ�󽻻���ʱ�������õ����µ���һ��ͼƬ����
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    VkSwapchainKHR prevSwapchain = mSwapchain;

    // �����������Ĳ���
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    // ע��ع���һ�µ����ݣ������SurfaceFormatҪ����һ�»�ȡ��Surface Formatһ��
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
    // ����������
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
    // �鿴��ǰ������֧�ֶ�����ͼƬ������ȡ��ЩͼƬ��������˫����Ҳ������������ȵ�
    // ������������ʱ���ͬʱ����ͼƬ������ֱ����ȥ��������
    vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, nullptr);
    mSwapchainImages.resize(mImageCount);
    vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, mSwapchainImages.data());
    // �ж���ͼƬ�ʹ�������ImageViews��һһ��Ӧ
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
    // �������ж���ͼƬ�ʹ�������Fences
    // Fences�ǳ�����Ⱦʱ��������ͬ���õģ������ר�Ž�
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
    // ����ImageViews
    for (auto& view: mSwapchainImageViews)
    {
        vkDestroyImageView(mLogicalDevice, view, VK_NULL_HANDLE);
    }

    // ע�⣺��������VkImages����Ҫ�ֶ����٣��ᱨ����������뱻ע����ֻ��Ϊ��չʾһ�£������ʵ�ʿ�����ʱ�����ôд����
    //for (auto& image: mSwapchainImages)
    //{
    //    vkDestroyImage(mLogicalDevice, image, VK_NULL_HANDLE);
    //}
    // ��������ͬ����Fences
    for (auto& fence: mSwapchainFences)
    {
        vkDestroyFence(mLogicalDevice, fence, VK_NULL_HANDLE);
    }
    // ���ѽ�������ʱ���丽����VKImages���Զ��������ˣ�
    vkDestroySwapchainKHR(mLogicalDevice, mSwapchain, VK_NULL_HANDLE);
}
