#include "Surface.h"

Surface::Surface(VkInstance& instance, GLFWwindow* window,
    VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamilyIndex)
{
    // �ڴ����ϴ���һ��Surface
    auto error = glfwCreateWindowSurface(instance, window, nullptr, &mSurface);
    assert(error == VK_SUCCESS);

    // �鿴��ǰ������Surface�Ƿ�������ʾͼ�Ρ���
    // ����������Ǹ��ϻ���������ΪVulkan���ƵĽ��ȷʵ����Ҫ�����ʾ��������Щ�豸�Ͳ�������ʾ
    VkBool32 supportPresent = VK_FALSE;
    error = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueFamilyIndex, mSurface, &supportPresent);
    assert(error == VK_SUCCESS && supportPresent);

    // ��ȡ��ǰSurface֧�ֵ���ɫ���ͣ����Ҽ�¼����
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, surfaceFormats.data());

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        mSurfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        mSurfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
    }
    else
    {
        bool found = false;
        for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                mSurfaceFormat = surfaceFormat;
                found = true;
                break;
            }
        }
        if (!found)
        {
            mSurfaceFormat = surfaceFormats[0];
        }
    }
}
