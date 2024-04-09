#pragma once
#include <iostream>
#include <vector>
#include <Volk/volk.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Include/ResourceLimits.h>

#include "Common.h"

class ShaderIncluder: public glslang::TShader::Includer
{
	std::vector<IncludeResult> mIncludeResult;
	std::vector<std::string> mHeaderData;

	IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override;

	void releaseInclude(IncludeResult*) override;
};

class ShaderModule: public IDisposable
{
public:
	ShaderModule(VkDevice logicalDevice, EShLanguage stage, const char* shaderSource);
	ShaderModule(VkDevice logicalDevice, EShLanguage stage, const std::string& shaderSource);
	ShaderModule(VkDevice logicalDevice, EShLanguage stage, const std::vector<char>& shaderSource);

	VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);

	virtual ~ShaderModule();
	void Dispose() override;

	VkShaderModule& GetShaderModule();
private:
	std::vector<uint32_t> mSPIRVCode;
	VkShaderModule mShaderModule;
	const VkDevice mLogicalDevice;
	bool mIsValid = false;
};

