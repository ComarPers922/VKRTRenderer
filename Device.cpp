#include "Device.h"

#include "Buffer.h"

VkPhysicalDevice Device::PhysicalDevice;
VkDevice Device::LogicalDevice;
bool Device::IsInited = false;

VkQueue Device::GraphicsQueue;
VkQueue Device::ComputeQueue;
VkQueue Device::TransferQueue;

Queue Device::Queue;

VkPhysicalDeviceRayTracingPipelinePropertiesKHR Device::RTProps;

void Device::Init(VkInstance& instance)
{
	if (IsInited)
	{
		return;
	}
    
    InitPhysicalDevice(instance);
    InitQueue();
    InitLogicalDevice();
}

void Device::InitPhysicalDevice(VkInstance& instance)
{
    // 获取当前有多少物理设备
    uint32_t numPhysDevices = 0;
    VkResult error = vkEnumeratePhysicalDevices(instance, &numPhysDevices, nullptr);

    assert(VK_SUCCESS == error && numPhysDevices);

    // 获取所有物理设备
    std::vector<VkPhysicalDevice> totalPhysicalDevices;
    totalPhysicalDevices.resize(numPhysDevices, nullptr);
    vkEnumeratePhysicalDevices(instance, &numPhysDevices, totalPhysicalDevices.data());

    VkDeviceSize deviceSize = 0;
    std::cout << "Available devices: " << std::endl;
    // 这里通过一个简单粗暴的方式获取设备：选显存最大的那个
    for (const auto& physicalDevice : totalPhysicalDevices)
    {
        if (physicalDevice == nullptr)
        {
            continue;
        }
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
        // 获取设备的物理属性
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
        // 获取其物理属性中描述显存的那个部分
        auto currentDeviceSize = deviceMemoryProperties.memoryHeaps[VK_MEMORY_HEAP_DEVICE_LOCAL_BIT].size;
        if (currentDeviceSize > deviceSize && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            PhysicalDevice = physicalDevice;
            deviceSize = currentDeviceSize;
        }
        // 打印当前设备名称
        std::cout << deviceProperties.deviceName << std::endl;
    }
    // PhysicalDevice是设备单例类的一个成员，全局可访问
    if (!PhysicalDevice)
    {
        PhysicalDevice = totalPhysicalDevices.front();
    }

    // 获取当前设备与光线追踪管线有关的属性
    RTProps = {};
    RTProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 devProps;
    devProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    devProps.pNext = &RTProps;
    devProps.properties = {};

    vkGetPhysicalDeviceProperties2(PhysicalDevice, &devProps);
}

void Device::InitQueue()
{
    // 这里记录我们需要哪些种类的队列
    const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
    uint32_t queuesIndices[3] = { ~0u, ~0u, ~0u };

    // 获取当前设备一共有多少队列
    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (size_t i = 0; i < 3; ++i)
    {
        // 获取当前需要找到的那个队列
        const VkQueueFlagBits& flag = askingFlags[i];
        uint32_t& queueIdx = queuesIndices[i];

        if (flag == VK_QUEUE_COMPUTE_BIT)
        {
            // 如果当前的队列只能做计算
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
            {
                if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                    !(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    queueIdx = j;
                    break;
                }
            }
        }
        else if (flag == VK_QUEUE_TRANSFER_BIT)
        {
        	// 如果当前的队列只能做数据传输
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
            {
                if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    !(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT))
                {
                    queueIdx = j;
                    break;
                }
            }
        }

        // 能走到这里则说明如果当前的队列可以同时干三件事情
        if (queueIdx == ~0u)
        {
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
            {
                if (queueFamilyProperties[j].queueFlags & flag)
                {
                    queueIdx = j;
                    break;
                }
            }
        }
    }

    Queue.GraphicsQueueFamilyIndex = queuesIndices[0];
    Queue.ComputeQueueFamilyIndex = queuesIndices[1];
    Queue.TransferQueueFamilyIndex = queuesIndices[2];
}

void Device::InitLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    const float priority = 0.0f;

    // 这里定义了我们希望逻辑设备可以支持哪些队列
    // 对于很多显卡来说，图形、计算和数据传输都是一个Queue Family，不过我们依然不能排除三件事在不同的Queue Family的情况
    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = Queue.GraphicsQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &priority;
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    // 如果Compute QueueFamily是一个单独的Index
    if (Queue.ComputeQueueFamilyIndex != Queue.GraphicsQueueFamilyIndex)
    {
        deviceQueueCreateInfo.queueFamilyIndex = Queue.ComputeQueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }
    if (Queue.TransferQueueFamilyIndex != Queue.GraphicsQueueFamilyIndex
        && Queue.TransferQueueFamilyIndex != Queue.ComputeQueueFamilyIndex)
    {
        deviceQueueCreateInfo.queueFamilyIndex = Queue.TransferQueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    // 这里需要指定我们要创建的逻辑设备需要支持光线追踪管线与加速结构
    // 所以我们需要指定当前逻辑设备需要加载哪些扩展
    VkPhysicalDeviceFeatures2 features2 = { };
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = {};
    rayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingStructure = {};
    rayTracingStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = {};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    std::vector<const char*> deviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
    rayTracingPipeline.pNext = &bufferDeviceAddress;
    rayTracingPipeline.rayTracingPipeline = VK_TRUE;
    rayTracingStructure.pNext = &rayTracingPipeline;
    rayTracingStructure.accelerationStructure = VK_TRUE;
    features2.pNext = &rayTracingStructure;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = { };
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    if (features2.pNext)
    {
        descriptorIndexing.pNext = features2.pNext;
    }

    features2.pNext = &descriptorIndexing;
    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &features2); // 把显卡所有的特性全部激活

    // 开始创建设备
    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;

    auto error = vkCreateDevice(PhysicalDevice, &deviceCreateInfo, nullptr, &LogicalDevice);
    assert(VK_SUCCESS == error);

    // 获取到实际处理图形、计算和传输的Handles（可以理解为指针）
    // 之后实际要把CommandBuffer推送给显卡，需要指定哪个队列的时候，就需要使用这些Handles
    vkGetDeviceQueue(LogicalDevice, Queue.GraphicsQueueFamilyIndex, 0, &GraphicsQueue);
    vkGetDeviceQueue(LogicalDevice, Queue.ComputeQueueFamilyIndex, 0, &ComputeQueue);
    vkGetDeviceQueue(LogicalDevice, Queue.TransferQueueFamilyIndex, 0, &TransferQueue);
}

VkDeviceOrHostAddressKHR Device::GetBufferDeviceAddress(const Buffer& buffer)
{
	VkBufferDeviceAddressInfoKHR info = {
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		nullptr,
		buffer.GetVkBuffer(),
	};

    VkDeviceOrHostAddressKHR result;
    result.deviceAddress = vkGetBufferDeviceAddressKHR(LogicalDevice, &info);

    return result;
}
