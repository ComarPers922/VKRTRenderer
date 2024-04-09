#include "TopLevelAccelerationStructure.h"
#include <vector>

TopLevelAccelerationStructure::TopLevelAccelerationStructure(VmaAllocator& allocator):
	mAllocator(allocator)
{
    mAccelerationStructure.buffer = std::make_shared<Buffer>(mAllocator);
}

void TopLevelAccelerationStructure::Build(
    VkDevice& logicalDevice,
    VkCommandPool& cmdPool,
    VkQueue& queue,
	std::vector<std::shared_ptr<Mesh>> meshes)
{
    mLogicalDevice = logicalDevice;
    const size_t numMeshes = meshes.size();

    // Ŀǰ��һ��Mesh��Ӧһ��Instance 
    std::vector<VkAccelerationStructureInstanceKHR> instances(numMeshes, VkAccelerationStructureInstanceKHR{});
    for (size_t i = 0; i < numMeshes; ++i)
    {
        auto& mesh = meshes[i];

        VkAccelerationStructureInstanceKHR& instance = instances[i];
        // ������Ҫָ��ÿ��Instance��Transform��Ϣ
        auto& meshTransform = mesh->GetTransform();

        for (size_t row = 0; row < 3; row++)
        {
            for (size_t col = 0; col < 4; col++)
            {
                instance.transform.matrix[row][col] = meshTransform[row][col];
            }
        }
        // ������Ҫ��ÿ��Instanceָ��һ��������ID��������Shader�������ֵ�ǰ���ǲ��������Ǹ�����
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        // ����ָ��Instance��Mask��������Shader�з������ߵ�ʱ�������ײ���ֶ���
        // ����Ŀǰ��Demo��û��������
        instance.mask = 0x01;
        if (mesh->GetMeshType() == WINDOW)
        {
            instance.mask = 0x02;
        }
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = mesh->GetAccelerationStructure().handle;
    }
    // ����Instance��Buffer
    Buffer instancesBuffer(mAllocator);
    VkResult error = instancesBuffer.CreateBuffer(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    
    assert(error == VK_SUCCESS);
    // �ϴ�Instance����
    instancesBuffer.UploadData(instances.data(), instancesBuffer.GetSize());

    // ��ʼ����������ٽṹ����ʵ���������̺͵ײ���ٽṹ������
    VkAccelerationStructureGeometryInstancesDataKHR tlasInstancesInfo = {};
    tlasInstancesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasInstancesInfo.data = GetBufferDeviceAddressConst(logicalDevice, instancesBuffer);

    VkAccelerationStructureGeometryKHR  tlasGeoInfo = {};
    tlasGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeoInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeoInfo.geometry.instances = tlasInstancesInfo;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeoInfo;

    const uint32_t numInstances = static_cast<uint32_t>(instances.size());
    // ����������ٽṹ��ʱ���䴴����С����Ҫ������ѯһ�����������������ֵ��ֻ��Ҫ��Vulkan�������Ǿ�����
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &numInstances, &sizeInfo);

	mAccelerationStructure.buffer->CreateBuffer(sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.buffer = mAccelerationStructure.buffer->GetVkBuffer();
    // �������ٽṹ����ʱ��û��������ʵ�ʴ�������
    error = vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &mAccelerationStructure.accelerationStructure);
    assert(error == VK_SUCCESS);
    // ��Ȼ��Ҫһ����ʱ��Buffer
    Buffer scratchBuffer(mAllocator);
    error = scratchBuffer.CreateBuffer(sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    assert(error == VK_SUCCESS);

    buildInfo.scratchData = GetBufferDeviceAddress(logicalDevice, scratchBuffer);
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = mAccelerationStructure.accelerationStructure;

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = cmdPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
    assert(error == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount = numInstances;

    const VkAccelerationStructureBuildRangeInfoKHR* ranges[1] = { &range };

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, ranges);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    // ��ʼ����������ٽṹ
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    error = vkQueueWaitIdle(queue);
    ASSERT_VK(error);
    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &commandBuffer);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = mAccelerationStructure.accelerationStructure;
    mAccelerationStructure.handle = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);
    // �ͷ���ʱ����
    scratchBuffer.Free();
    instancesBuffer.Free();
}

void TopLevelAccelerationStructure::Dispose()
{
    mAccelerationStructure.buffer->Free();
    vkDestroyAccelerationStructureKHR(mLogicalDevice, mAccelerationStructure.accelerationStructure, VK_NULL_HANDLE);
}
