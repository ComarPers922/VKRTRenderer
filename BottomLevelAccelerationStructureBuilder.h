#pragma once
#include "Common.h"

#include "Mesh.h"

// It builds the bottom level acceleration structure for each mesh, and then it stores the result in each mesh.
class BottomLevelAccelerationStructureBuilder
{
public:
	BottomLevelAccelerationStructureBuilder(VmaAllocator&);
	void Build(VkDevice& logicalDevice,
		VkCommandPool& cmdPool,
		VkQueue& graphicsQueue,
		std::vector<std::shared_ptr<Mesh>>& meshes);
private:
	VmaAllocator& mVmaAllocator;
};

