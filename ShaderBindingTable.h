#pragma once

#include "Buffer.h"
#include "Common.h"

class ShaderBindingTable
{

public:
	ShaderBindingTable(VmaAllocator& allocator);
	~ShaderBindingTable() = default;

	void Initialize(const uint32_t& numHitGroups,
		const uint32_t& numMissGroups,
		const uint32_t& shaderHandleSize,
		const uint32_t& shaderGroupAlignment);

	void SetRaygenStage(const VkPipelineShaderStageCreateInfo& stage);
	void AddStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex);
	void AddStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex);

	uint32_t GetGroupsStride() const;
	uint32_t GetNumGroups() const;
	uint32_t GetRaygenOffset() const;
	uint32_t GetRaygenSize() const;
	uint32_t GetHitGroupsOffset() const;
	uint32_t GetHitGroupsSize() const;
	uint32_t GetMissGroupsOffset() const;
	uint32_t GetMissGroupsSize() const;

	uint32_t GetNumStages() const;

	const VkPipelineShaderStageCreateInfo* GetStages() const;
	const VkRayTracingShaderGroupCreateInfoKHR* GetGroups() const;

	uint32_t GetSBTSize() const;
	void CreateSBT(VkDevice device, VkPipeline rtPipeline);
	VkDeviceAddress GetSBTAddress();

	void Dispose();

private:
	uint32_t mShaderHandleSize;
	uint32_t mShaderGroupAlignment;
	uint32_t mNumHitGroups;
	uint32_t mNumMissGroups;
	std::vector<uint32_t> mNumHitShaders;
	std::vector<uint32_t> mNumMissShaders;
	std::vector<VkPipelineShaderStageCreateInfo> mStages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> mGroups;
	Buffer mShaderBindingTableBuffer;
	VkDevice mLogicalDevice;
};

