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
    // ��ȡ��ǰ�ж��������豸
    uint32_t numPhysDevices = 0;
    VkResult error = vkEnumeratePhysicalDevices(instance, &numPhysDevices, nullptr);

    assert(VK_SUCCESS == error && numPhysDevices);

    // ��ȡ���������豸
    std::vector<VkPhysicalDevice> totalPhysicalDevices;
    totalPhysicalDevices.resize(numPhysDevices, nullptr);
    vkEnumeratePhysicalDevices(instance, &numPhysDevices, totalPhysicalDevices.data());

    VkDeviceSize deviceSize = 0;
    std::cout << "Available devices: " << std::endl;
    // ����ͨ��һ���򵥴ֱ��ķ�ʽ��ȡ�豸��ѡ�Դ������Ǹ�
    for (const auto& physicalDevice : totalPhysicalDevices)
    {
        if (physicalDevice == nullptr)
        {
            continue;
        }
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
        // ��ȡ�豸����������
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
        // ��ȡ�����������������Դ���Ǹ�����
        auto currentDeviceSize = deviceMemoryProperties.memoryHeaps[VK_MEMORY_HEAP_DEVICE_LOCAL_BIT].size;
        if (currentDeviceSize > deviceSize && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            PhysicalDevice = physicalDevice;
            deviceSize = currentDeviceSize;
        }
        // ��ӡ��ǰ�豸����
        std::cout << deviceProperties.deviceName << std::endl;
    }
    // PhysicalDevice���豸�������һ����Ա��ȫ�ֿɷ���
    if (!PhysicalDevice)
    {
        PhysicalDevice = totalPhysicalDevices.front();
    }

    // ��ȡ��ǰ�豸�����׷�ٹ����йص�����
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
    // �����¼������Ҫ��Щ����Ķ���
    const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
    uint32_t queuesIndices[3] = { ~0u, ~0u, ~0u };

    // ��ȡ��ǰ�豸һ���ж��ٶ���
    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (size_t i = 0; i < 3; ++i)
    {
        // ��ȡ��ǰ��Ҫ�ҵ����Ǹ�����
        const VkQueueFlagBits& flag = askingFlags[i];
        uint32_t& queueIdx = queuesIndices[i];

        if (flag == VK_QUEUE_COMPUTE_BIT)
        {
            // �����ǰ�Ķ���ֻ��������
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
        	// �����ǰ�Ķ���ֻ�������ݴ���
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

        // ���ߵ�������˵�������ǰ�Ķ��п���ͬʱ����������
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

    // ���ﶨ��������ϣ���߼��豸����֧����Щ����
    // ���ںܶ��Կ���˵��ͼ�Ρ���������ݴ��䶼��һ��Queue Family������������Ȼ�����ų��������ڲ�ͬ��Queue Family�����
    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = Queue.GraphicsQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &priority;
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    // ���Compute QueueFamily��һ��������Index
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

    // ������Ҫָ������Ҫ�������߼��豸��Ҫ֧�ֹ���׷�ٹ�������ٽṹ
    // ����������Ҫָ����ǰ�߼��豸��Ҫ������Щ��չ
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
    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &features2); // ���Կ����е�����ȫ������

    // ��ʼ�����豸
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

    // ��ȡ��ʵ�ʴ���ͼ�Ρ�����ʹ����Handles���������Ϊָ�룩
    // ֮��ʵ��Ҫ��CommandBuffer���͸��Կ�����Ҫָ���ĸ����е�ʱ�򣬾���Ҫʹ����ЩHandles
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
