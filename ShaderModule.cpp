#include <filesystem>

#include "ShaderModule.h"

#include "FileUtility.h"
#include "Constants.h"
using Includer = glslang::TShader::Includer;

const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,
	/* .maxMeshOutputVerticesEXT = */ 256,
	/* .maxMeshOutputPrimitivesEXT = */ 256,
	/* .maxMeshWorkGroupSizeX_EXT = */ 128,
	/* .maxMeshWorkGroupSizeY_EXT = */ 128,
	/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
	/* .maxTaskWorkGroupSizeX_EXT = */ 128,
	/* .maxTaskWorkGroupSizeY_EXT = */ 128,
	/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
	/* .maxMeshViewCountEXT = */ 4,
	/* .maxDualSourceDrawBuffersEXT = */ 1,

	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	} };

Includer::IncludeResult* ShaderIncluder::includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth)
{
	std::filesystem::path relativePath = (std::string(DEFAULT_SHADER_DIR) + std::string(headerName)).c_str();
	auto absolutePath = absolute(relativePath).generic_string();
	auto fileData = FileUtility::readFile(std::string(DEFAULT_SHADER_DIR) + std::string(headerName));

	auto curFileData = fileData.data();
	mHeaderData.emplace_back(curFileData);

	mIncludeResult.emplace_back(absolutePath,
		mHeaderData.back().c_str(),
		mHeaderData.back().size(), this);
	return &mIncludeResult.back();
}

void ShaderIncluder::releaseInclude(IncludeResult* result)
{
	
}

ShaderModule::ShaderModule(VkDevice logicalDevice, EShLanguage stage, const char* shaderSource) :
	mLogicalDevice(logicalDevice)
{
	// 声明Shader的Stage、源代码和一些参数
	glslang::TShader shader(stage);
	shader.setStrings(&shaderSource, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 120);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
	// 使用includer把所有的包含#include内容的源代码加载出来
	ShaderIncluder includer;
	if (!shader.parse(&DefaultTBuiltInResource, 120, ENoProfile, false, false, EShMsgDefault, includer))
	{
		std::cerr << shader.getInfoLog();
		return;
	}

	// 开始编译
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(EShMsgDefault))
	{
		std::cerr << program.getInfoLog();
		return;
	}
	const auto intermediate = program.getIntermediate(stage);

	glslang::GlslangToSpv(*intermediate, mSPIRVCode);

	// 创建Vulkan的ShaderModule
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = mSPIRVCode.size() * sizeof(unsigned int);
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(mSPIRVCode.data());

	auto result = vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo, nullptr, &mShaderModule);
	if (result != VK_SUCCESS)
	{
		return;
	}

	mIsValid = true;
}

ShaderModule::ShaderModule(VkDevice logicalDevice, EShLanguage stage, const std::string& shaderSource)
	: ShaderModule(logicalDevice, stage, shaderSource.c_str())
{

}

ShaderModule::ShaderModule(VkDevice logicalDevice, EShLanguage stage, const std::vector<char>& shaderSource)
	: ShaderModule(logicalDevice, stage, shaderSource.data())
{

}

VkPipelineShaderStageCreateInfo ShaderModule::GetShaderStage(VkShaderStageFlagBits stage)
{
	return VkPipelineShaderStageCreateInfo{
		/*sType*/ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		/*pNext*/ nullptr,
		/*flags*/ 0,
		/*stage*/ stage,
		/*module*/ mShaderModule,
		/*pName*/ "main",
		/*pSpecializationInfo*/ nullptr
	};
}

ShaderModule::~ShaderModule()
{
	Dispose();
}

void ShaderModule::Dispose()
{
	if (!mIsValid)
	{
		return;
	}
	vkDestroyShaderModule(mLogicalDevice, mShaderModule, VK_NULL_HANDLE);
	mIsValid = false;
}

VkShaderModule& ShaderModule::GetShaderModule()
{
	return mShaderModule;
}
