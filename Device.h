#pragma once
#include "Common.h"

struct Queue
{
	uint32_t GraphicsQueueFamilyIndex;
	uint32_t ComputeQueueFamilyIndex;
	uint32_t TransferQueueFamilyIndex;
};

class Device
{
public:
	Device() = delete;
	~Device() = delete;

	static void Init(VkInstance& instance);

	STATIC_INLINE_GETTER(VkPhysicalDevice, PhysicalDevice);
	STATIC_INLINE_GETTER(VkDevice, LogicalDevice);
	STATIC_INLINE_GETTER(Queue, Queue);
	STATIC_INLINE_GETTER(VkPhysicalDeviceRayTracingPipelinePropertiesKHR, RTProps);

	STATIC_INLINE_GETTER(VkQueue, GraphicsQueue);
	STATIC_INLINE_GETTER(VkQueue, ComputeQueue);
	STATIC_INLINE_GETTER(VkQueue, TransferQueue);

	static VkDeviceOrHostAddressKHR GetBufferDeviceAddress(const Buffer& buffer);

private:
	static VkPhysicalDevice PhysicalDevice;
	static VkDevice LogicalDevice;
	static bool IsInited;
	static Queue Queue;

	static VkQueue GraphicsQueue;
	static VkQueue ComputeQueue;
	static VkQueue TransferQueue;

	static VkPhysicalDeviceRayTracingPipelinePropertiesKHR RTProps;

	static void InitPhysicalDevice(VkInstance& instance);
	static void InitQueue();
	static void InitLogicalDevice();
};

