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

    // 目前是一个Mesh对应一个Instance 
    std::vector<VkAccelerationStructureInstanceKHR> instances(numMeshes, VkAccelerationStructureInstanceKHR{});
    for (size_t i = 0; i < numMeshes; ++i)
    {
        auto& mesh = meshes[i];

        VkAccelerationStructureInstanceKHR& instance = instances[i];
        // 这里需要指定每个Instance的Transform信息
        auto& meshTransform = mesh->GetTransform();

        for (size_t row = 0; row < 3; row++)
        {
            for (size_t col = 0; col < 4; col++)
            {
                instance.transform.matrix[row][col] = meshTransform[row][col];
            }
        }
        // 这里需要给每个Instance指定一个独立的ID，用于在Shader里面区分当前我们操作的是那个对象
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        // 这里指定Instance的Mask，用于在Shader中发射射线的时候忽略碰撞部分对象
        // 不过目前的Demo并没有做区分
        instance.mask = 0x01;
        if (mesh->GetMeshType() == WINDOW)
        {
            instance.mask = 0x02;
        }
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = mesh->GetAccelerationStructure().handle;
    }
    // 创建Instance的Buffer
    Buffer instancesBuffer(mAllocator);
    VkResult error = instancesBuffer.CreateBuffer(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    
    assert(error == VK_SUCCESS);
    // 上传Instance数据
    instancesBuffer.UploadData(instances.data(), instancesBuffer.GetSize());

    // 开始创建顶层加速结构，其实创建的流程和底层加速结构很类似
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
    // 创建顶层加速结构的时候，其创建大小不需要我们轮询一遍整个集合来找最大值，只需要让Vulkan告诉我们就行了
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
    // 创建加速结构，此时还没有往里面实际传入数据
    error = vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &mAccelerationStructure.accelerationStructure);
    assert(error == VK_SUCCESS);
    // 依然需要一个临时的Buffer
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
    // 开始构建顶层加速结构
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    error = vkQueueWaitIdle(queue);
    ASSERT_VK(error);
    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &commandBuffer);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = mAccelerationStructure.accelerationStructure;
    mAccelerationStructure.handle = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);
    // 释放临时缓存
    scratchBuffer.Free();
    instancesBuffer.Free();
}

void TopLevelAccelerationStructure::Dispose()
{
    mAccelerationStructure.buffer->Free();
    vkDestroyAccelerationStructureKHR(mLogicalDevice, mAccelerationStructure.accelerationStructure, VK_NULL_HANDLE);
}
