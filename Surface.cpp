#include "Surface.h"

Surface::Surface(VkInstance& instance, GLFWwindow* window,
    VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamilyIndex)
{
    // 在窗口上创建一个Surface
    auto error = glfwCreateWindowSurface(instance, window, nullptr, &mSurface);
    assert(error == VK_SUCCESS);

    // 查看当前创建的Surface是否允许显示图形……
    // 这个看起来是个废话，不过因为Vulkan绘制的结果确实不需要真的显示出来，有些设备就不允许显示
    VkBool32 supportPresent = VK_FALSE;
    error = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueFamilyIndex, mSurface, &supportPresent);
    assert(error == VK_SUCCESS && supportPresent);

    // 获取当前Surface支持的颜色类型，并且记录下来
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
