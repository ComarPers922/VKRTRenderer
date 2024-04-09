#pragma once
#include "Mesh.h"

class TopLevelAccelerationStructure
{
public:
	TopLevelAccelerationStructure(VmaAllocator&);
	void Build(VkDevice& logicalDevice,
		VkCommandPool& cmdPool,
		VkQueue& queue,
		std::vector<std::shared_ptr<Mesh>> meshes);

	[[nodiscard]]
	const AccelerationStructure& GetAccelerationStructure() const
	{
		return mAccelerationStructure;
	}

	void Dispose();
private:
	VmaAllocator& mAllocator;
	AccelerationStructure mAccelerationStructure;

	VkDevice mLogicalDevice;
};

