#pragma once
#include <Volk/volk.h>

#include "Swapchain.h"

/*
 * There is no render pass for ray tracing pipeline, so we need a
 * special render pass for ImGUI.
 * The render pass declared below is not suitable for normal rendering operation.
 * It is specially for ImGUI.
 */
class ImGUIRenderPass
{
public:
	ImGUIRenderPass(const VkDevice& logicalDevice);
	[[nodiscard]]
	VkRenderPass GetVkRenderPass() const
	{
		return mRenderPass;
	}

	void Dispose();
private:
	VkRenderPass mRenderPass;
	VkDevice mLogicalDevice;
};

