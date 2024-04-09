#pragma once
#include "Common.h"

class Surface
{
public:
	Surface(VkInstance& instance, GLFWwindow* window,
		VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamilyIndex);

	[[nodiscard]]
	VkSurfaceKHR& GetSurface()
	{
		return mSurface;
	}
	[[nodiscard]]
	VkSurfaceFormatKHR& GetSurfaceFormat()
	{
		return mSurfaceFormat;
	}
private:
	VkSurfaceKHR mSurface;
	VkSurfaceFormatKHR mSurfaceFormat;
};

