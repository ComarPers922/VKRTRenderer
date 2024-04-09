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
	// ��ȡ��ǰ������Ҫ��Ⱦ��ģ������
	const size_t numMeshes = meshes.size();
	std::vector geometries(numMeshes, VkAccelerationStructureGeometryKHR{});
	std::vector ranges(numMeshes, VkAccelerationStructureBuildRangeInfoKHR{});
	std::vector buildInfos(numMeshes, VkAccelerationStructureBuildGeometryInfoKHR{});
	std::vector sizeInfos(numMeshes, VkAccelerationStructureBuildSizesInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR });

	// ����һ��ScratchBuffer��������ʱ���漸������
	Buffer scratchBuffer(mVmaAllocator);
	// ������Ҫ֪�����Ǽ��������ļ����������У��ĸ���ռ�����Ŀռ�
	// ��ΪScratchBuffer�ᱻ����ʹ�ã�ֱ�����еײ���ٽṹ���������
	VkDeviceSize maximumBlasSize = 0;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	// ��ʼ����ÿ��Mesh����
	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& mesh = meshes[i];

		VkAccelerationStructureGeometryKHR& geometry = geometries[i];
		VkAccelerationStructureBuildRangeInfoKHR& range = ranges[i];
		VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
		// ����ָ���˵�ǰMesh�ж���������
		range.primitiveCount = mesh->GetIndexCount() / FACE_NUM;

		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		// ����ָ��Mesh��Vertices����
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = GetBufferDeviceAddressConst(logicalDevice, mesh->GetPositionBuffer());
		geometry.geometry.triangles.vertexStride = sizeof(vec3);
		geometry.geometry.triangles.maxVertex = mesh->GetPositionCount();
		// ����ָ��Mesh��Index����
		geometry.geometry.triangles.indexData = GetBufferDeviceAddressConst(logicalDevice, mesh->GetIndexBuffer());
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

		// ������Ĭ�ϲ����Ϳ�����
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		// �����Demo�У����������ڹ��������ļ��ٽṹ�ܹ���������ײ��ʱ�����Ч��
		buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;

		// ��ȡ�����Ҫ������ǰ�ĵײ���ٽṹ��Ҫ���Ļ���
		vkGetAccelerationStructureBuildSizesKHR(logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfo,
			&range.primitiveCount,
			&sizeInfos[i]);

	}
	// ��¼�����Ǹ�����
	for (const auto& sizeInfo : sizeInfos)
	{
		maximumBlasSize = std::max(sizeInfo.buildScratchSize, maximumBlasSize);
	}

	// ������Size��������
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

	// �趨һ���ڴ����ϣ��Ա�֤���ٽṹ�ܹ���������
	VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& mesh = meshes[i];

		// ׼�������ײ���ٽṹ
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
		// �����ײ���ٽṹ
		vkResult = vkCreateAccelerationStructureKHR(logicalDevice,
			&createInfo,
			nullptr,
			&acclerationStructure.accelerationStructure);
		assert(vkResult == VK_SUCCESS);

		// ��ʼ�����ײ���ٽṹ
		// ע�⣺����Ǵ������ٽṹ����ҪCommandBuffer��������ǹ���������ҪCommandBuffer
		VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
		buildInfo.scratchData = Device::GetBufferDeviceAddress(scratchBuffer);
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = acclerationStructure.accelerationStructure;

		const VkAccelerationStructureBuildRangeInfoKHR* range[1] = { &ranges[i] };
		// ��ʼ����
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, range);

		// ʹ��MemoryBuffer���Է�ֹ�����������ļ��ٽṹ����ǰ��ScratchBuffer
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

	// �ѹ����õļ��ٽṹ��Mesh
	for (size_t i = 0; i < numMeshes; ++i) 
	{
		auto& mesh = meshes[i];
		auto& accelerationStructure = mesh->GetAccelerationStructure();
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = accelerationStructure.accelerationStructure;
		accelerationStructure.handle = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);
	}

	// �ͷ���ʱ����
	scratchBuffer.Free();
}
