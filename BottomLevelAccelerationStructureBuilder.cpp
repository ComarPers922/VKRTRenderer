#include "BottomLevelAccelerationStructureBuilder.h"
#include <utility>
#include "Device.h"

BottomLevelAccelerationStructureBuilder::BottomLevelAccelerationStructureBuilder(VmaAllocator& allocator)
	: // mBuffer(allocator),
	// handle(0),
	mVmaAllocator(allocator)
{

}

void BottomLevelAccelerationStructureBuilder::Build(
	VkDevice& logicalDevice,
	VkCommandPool& cmdPool,
	VkQueue& graphicsQueue,
	std::vector<std::shared_ptr<Mesh>>& meshes)
{
	// 获取当前场景需要渲染的模型数量
	const size_t numMeshes = meshes.size();
	std::vector geometries(numMeshes, VkAccelerationStructureGeometryKHR{});
	std::vector ranges(numMeshes, VkAccelerationStructureBuildRangeInfoKHR{});
	std::vector buildInfos(numMeshes, VkAccelerationStructureBuildGeometryInfoKHR{});
	std::vector sizeInfos(numMeshes, VkAccelerationStructureBuildSizesInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR });

	// 创建一个ScratchBuffer，用来临时储存几何数据
	Buffer scratchBuffer(mVmaAllocator);
	// 我们需要知道我们即将构建的几个几何体中，哪个会占用最大的空间
	// 因为ScratchBuffer会被反复使用，直到所有底层加速结构均构建完毕
	VkDeviceSize maximumBlasSize = 0;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	// 开始对于每个Mesh操作
	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& mesh = meshes[i];

		VkAccelerationStructureGeometryKHR& geometry = geometries[i];
		VkAccelerationStructureBuildRangeInfoKHR& range = ranges[i];
		VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
		// 这里指定了当前Mesh有多少三角形
		range.primitiveCount = mesh->GetIndexCount() / FACE_NUM;

		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		// 这里指定Mesh的Vertices数据
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = GetBufferDeviceAddressConst(logicalDevice, mesh->GetPositionBuffer());
		geometry.geometry.triangles.vertexStride = sizeof(vec3);
		geometry.geometry.triangles.maxVertex = mesh->GetPositionCount();
		// 这里指定Mesh的Index数据
		geometry.geometry.triangles.indexData = GetBufferDeviceAddressConst(logicalDevice, mesh->GetIndexBuffer());
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

		// 这里用默认参数就可以了
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		// 在这个Demo中，我们倾向于构建出来的加速结构能够在射线碰撞的时候更有效率
		buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;

		// 获取如果需要构建当前的底层加速结构需要多大的缓存
		vkGetAccelerationStructureBuildSizesKHR(logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfo,
			&range.primitiveCount,
			&sizeInfos[i]);

	}
	// 记录最大的那个缓存
	for (const auto& sizeInfo : sizeInfos)
	{
		maximumBlasSize = std::max(sizeInfo.buildScratchSize, maximumBlasSize);
	}

	// 以最大的Size构建缓存
	auto vkResult = scratchBuffer.CreateBuffer(maximumBlasSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
	assert(vkResult == VK_SUCCESS);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = cmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	vkResult = vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
	assert(vkResult == VK_SUCCESS);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// 设定一个内存屏障，以保证加速结构能够完整构建
	VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& mesh = meshes[i];

		// 准备创建底层加速结构
		auto& acclerationStructure = mesh->GetAccelerationStructure();

		acclerationStructure.buffer = std::make_shared<Buffer>(mVmaAllocator);
		acclerationStructure.buffer->CreateBuffer(sizeInfos[i].accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

		VkAccelerationStructureCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = sizeInfos[i].accelerationStructureSize;
		createInfo.buffer = acclerationStructure.buffer->GetVkBuffer();
		// 创建底层加速结构
		vkResult = vkCreateAccelerationStructureKHR(logicalDevice,
			&createInfo,
			nullptr,
			&acclerationStructure.accelerationStructure);
		assert(vkResult == VK_SUCCESS);

		// 开始构建底层加速结构
		// 注意：如果是创建加速结构不需要CommandBuffer，但如果是构建，则需要CommandBuffer
		VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
		buildInfo.scratchData = Device::GetBufferDeviceAddress(scratchBuffer);
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = acclerationStructure.accelerationStructure;

		const VkAccelerationStructureBuildRangeInfoKHR* range[1] = { &ranges[i] };
		// 开始构建
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, range);

		// 使用MemoryBuffer，以防止后面来构建的加速结构抢当前的ScratchBuffer
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkResult = vkQueueWaitIdle(graphicsQueue);
	assert(vkResult == VK_SUCCESS);
	vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &commandBuffer);

	// 把构建好的加速结构给Mesh
	for (size_t i = 0; i < numMeshes; ++i) 
	{
		auto& mesh = meshes[i];
		auto& accelerationStructure = mesh->GetAccelerationStructure();
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = accelerationStructure.accelerationStructure;
		accelerationStructure.handle = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);
	}

	// 释放临时缓存
	scratchBuffer.Free();
}
