#include "ShaderBindingTable.h"

ShaderBindingTable::ShaderBindingTable(VmaAllocator& allocator):
	mShaderHandleSize(0u),
	mShaderGroupAlignment(0u),
	mNumHitGroups(0u),
	mNumMissGroups(0u),
	mShaderBindingTableBuffer(allocator)
{

}

void ShaderBindingTable::Initialize(
	const uint32_t& numHitGroups, const uint32_t& numMissGroups,
	const uint32_t& shaderHandleSize, const uint32_t& shaderGroupAlignment)
{
	mShaderHandleSize = shaderHandleSize;
	mShaderGroupAlignment = shaderGroupAlignment;
	mNumHitGroups = numHitGroups;
	mNumMissGroups = numMissGroups;

	mNumHitShaders.resize(numHitGroups, 0u);
	mNumMissShaders.resize(numMissGroups, 0u);

	mStages.clear();
	mGroups.clear();
}

void ShaderBindingTable::SetRaygenStage(const VkPipelineShaderStageCreateInfo& stage)
{
	// this shader stage should go first!
	assert(mStages.empty());
	mStages.push_back(stage);

	VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = 0;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	mGroups.push_back(groupInfo); // group 0 is always for raygen
}

void ShaderBindingTable::AddStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex)
{
	// raygen stage should go first!
	assert(!mStages.empty());

	assert(groupIndex < mNumHitShaders.size());
	assert(!stages.empty() && stages.size() <= 3);// only 3 hit shaders per group (intersection, any-hit and closest-hit)
	assert(mNumHitShaders[groupIndex] == 0);

	uint32_t offset = 1; // there's always raygen shader

	for (uint32_t i = 0; i <= groupIndex; ++i)
	{
		offset += mNumHitShaders[i];
	}

	auto itStage = mStages.begin() + offset;
	mStages.insert(itStage, stages.begin(), stages.end());

	VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

	for (size_t i = 0; i < stages.size(); i++)
	{
		const VkPipelineShaderStageCreateInfo& stageInfo = stages[i];
		const uint32_t shaderIdx = static_cast<uint32_t>(offset + i);

		if (stageInfo.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		{
			groupInfo.closestHitShader = shaderIdx;
		}
		else if (stageInfo.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
		{
			groupInfo.anyHitShader = shaderIdx;
		}
	};

	mGroups.insert((mGroups.begin() + 1 + groupIndex), groupInfo);

	mNumHitShaders[groupIndex] += static_cast<uint32_t>(stages.size());
}

void ShaderBindingTable::AddStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex)
{
	// raygen stage should go first!
	assert(!mStages.empty());

	assert(groupIndex < mNumMissShaders.size());
	assert(mNumMissShaders[groupIndex] == 0); // only 1 miss shader per group

	uint32_t offset = 1; // there's always raygen shader

	// now skip all hit shaders
	for (const uint32_t numHitShader : mNumHitShaders) {
		offset += numHitShader;
	}

	for (uint32_t i = 0; i <= groupIndex; ++i) {
		offset += mNumMissShaders[i];
	}

	mStages.insert(mStages.begin() + offset, stage);

	// group create info 
	VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
	groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = offset;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

	// group 0 is always for raygen, then go hit shaders
	mGroups.insert((mGroups.begin() + (groupIndex + 1 + mNumHitGroups)), groupInfo);

	mNumMissShaders[groupIndex]++;
}

uint32_t ShaderBindingTable::GetGroupsStride() const
{
	return mShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetNumGroups() const
{
	return 1 + mNumHitGroups + mNumMissGroups;
}

uint32_t ShaderBindingTable::GetRaygenOffset() const
{
	return 0;
}

uint32_t ShaderBindingTable::GetRaygenSize() const
{
	return mShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetHitGroupsOffset() const
{
	return this->GetRaygenOffset() + this->GetRaygenSize();
}

uint32_t ShaderBindingTable::GetHitGroupsSize() const
{
	return mNumHitGroups * mShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetMissGroupsOffset() const
{
	return this->GetHitGroupsOffset() + this->GetHitGroupsSize();
}

uint32_t ShaderBindingTable::GetMissGroupsSize() const
{
	return mNumMissGroups * mShaderGroupAlignment;
}

uint32_t ShaderBindingTable::GetNumStages() const
{
	return static_cast<uint32_t>(mStages.size());
}

const VkPipelineShaderStageCreateInfo* ShaderBindingTable::GetStages() const
{
	return mStages.data();
}

const VkRayTracingShaderGroupCreateInfoKHR* ShaderBindingTable::GetGroups() const
{
	return mGroups.data();
}

uint32_t ShaderBindingTable::GetSBTSize() const
{
	return this->GetNumGroups() * mShaderGroupAlignment;
}

void ShaderBindingTable::CreateSBT(VkDevice device, VkPipeline rtPipeline)
{
	mLogicalDevice = device;
	const size_t sbtSize = this->GetSBTSize();

	VkResult error = mShaderBindingTableBuffer.CreateBuffer(sbtSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	assert(error == VK_SUCCESS);

	// get shader group handles
	std::vector<uint8_t> groupHandles(this->GetNumGroups() * mShaderHandleSize);
	error = vkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, this->GetNumGroups(), groupHandles.size(), groupHandles.data());

	assert(error == VK_SUCCESS);

	// now we fill our SBT
	uint8_t* mem = static_cast<uint8_t*>(mShaderBindingTableBuffer.Map());
	for (size_t i = 0; i < this->GetNumGroups(); ++i)
	{
		memcpy(mem, groupHandles.data() + i * mShaderHandleSize, mShaderHandleSize);
		mem += mShaderGroupAlignment;
	}
	mShaderBindingTableBuffer.Unmap();
}

VkDeviceAddress ShaderBindingTable::GetSBTAddress()
{
	return GetBufferDeviceAddress(
		mLogicalDevice,
		mShaderBindingTableBuffer).deviceAddress;
}

void ShaderBindingTable::Dispose()
{
	mShaderBindingTableBuffer.Free();
}
