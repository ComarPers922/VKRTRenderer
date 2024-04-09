#pragma once
/*
 * 这里面包含了所有的和光线追踪管线渲染的代码，管线初始化顺序都在构造函数里面了
 */

#define VMA_VULKAN_VERSION 1002000

#include "Common.h"

#include "Image.h"
#include "Mesh.h"
#include "Surface.h"
#include "Swapchain.h"
#include "BottomLevelAccelerationStructureBuilder.h"
#include "Camera.h"
#include "ShaderModule.h"
#include "TopLevelAccelerationStructure.h"
#include "ShaderBindingTable.h"
#include "DescriptorSetLayout.h"
#include "ImGUIRenderPass.h"
#include "shared_with_shaders.h"

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_impl_glfw.h"
#include "ImGUI/imgui_impl_vulkan.h"

class DescriptorSet;
using vec2 = glm::highp_vec2;
using vec3 = glm::highp_vec3;
using vec4 = glm::highp_vec4;
using mat4 = glm::highp_mat4;
using quat = glm::highp_quat;

#define CHECK_VK_ERROR(_error, _message)		\
	do{                                          \
    if (VK_SUCCESS != (_error))					\
	{											\
        assert(false && (_message));             \
    }}while(false)

class VKRTApp
{
public:
	VKRTApp(GLFWwindow* window, const uint32_t& width, const uint32_t& height);

	virtual ~VKRTApp();

	const VmaAllocator& GetAllocator() const
	{
		return mVmaAllocator;
	}

	const VkCommandPool& GetCommandPool() const
	{
		return mCommandPool;
	}

	uint32_t GetWidth() const
	{
		return mWidth;
	}

	uint32_t GetHeight() const
	{
		return mHeight;
	}

	GLFWwindow* GetWindow() const
	{
		return mWindow;
	}

	VkInstance GetInstance() const
	{
		return mInstance;
	}

	const VkSurfaceFormatKHR& GetSurfaceFormat() const
	{
		return mSurfaceFormat;
	}

	void ProcessFrame(const double&);

	void MoveCamera(const float& side, const float& forward);
	void MoveCameraUpDown(const float&);
	void RotateCamera(const float& x, const float& y);
	void SetMousePressed(const bool& val)
	{
		mIsRotating = val;
	}

	void UpdateCameraBuffer();
private:
	GLFWwindow* mWindow;

	uint32_t mWidth, mHeight;

	void InitInstance();

	void InitDevice();

	VkResult InitVma();

	VkResult InitCommandPool();

	VkResult InitializeOffscreenImage();

	VkResult InitializeCommandBuffers();

	VkResult InitializeSynchronization();

	void AddASBinding();
	void AddOffscreenImageBinding();
	void AddCameraBinding();
	void CreatePipelineLayout();
	void CreateRayTracingPipeline();
	void UpdateDescriptorSets();
	void FillCommandBuffers();
	void FillCommandBuffer(VkCommandBuffer, const size_t&);

	void InitImGUI();

	VkInstance mInstance;

	std::unique_ptr<Surface> mSurface;

	std::unique_ptr<Swapchain> mSwapchain;

	VkSurfaceFormatKHR mSurfaceFormat{};

	VkCommandPool mCommandPool;

	std::vector<VkCommandBuffer> mCommandBuffers;

	VkSemaphore mSemaphoreImageAcquired;
	VkSemaphore mSemaphoreRenderFinished;
	VmaAllocator mVmaAllocator;

	std::unique_ptr<Image> mOffscreenImage;
	std::unique_ptr<Image> mSkyBoxImage;

	std::vector<std::shared_ptr<Mesh>> mMeshes;
	std::vector<std::shared_ptr<Buffer>> mMatIDsBuffers;

	std::unique_ptr<BottomLevelAccelerationStructureBuilder> mBtmLvlAccStructBuilder;
	std::unique_ptr<TopLevelAccelerationStructure> mTopLvlAccStruct;

	std::shared_ptr<ShaderModule> mRayGen;
	std::shared_ptr<ShaderModule> mRayHit;
	std::shared_ptr<ShaderModule> mRayMiss;
	std::shared_ptr<ShaderModule> mShadowRayHit;
	std::shared_ptr<ShaderModule> mShadowRayMiss;

	std::unique_ptr<ShaderBindingTable> mShaderBindingTable;

	std::unique_ptr<DescriptorSetLayout> mLayoutRaygen;
	std::unique_ptr<DescriptorSetLayout> mLayoutMaterialIDs;
	std::unique_ptr<DescriptorSetLayout> mLayoutVertices;
	std::unique_ptr<DescriptorSetLayout> mLayoutFaces;
	std::unique_ptr<DescriptorSetLayout> mLayoutTexs;
	std::unique_ptr<DescriptorSetLayout> mLayoutSkyBox;
	std::unique_ptr<DescriptorSetLayout> mLayoutColor;
	std::unique_ptr<DescriptorSetLayout> mLayoutObjAttris;

	std::unique_ptr<DescriptorSet> mDescriptorSet;

	VkPipelineLayout mRTPipelineLayout;
	VkPipeline mRTPipeline;

	Camera mCamera;
	UniformParams mParams;
	std::unique_ptr<Buffer> mCameraBuffer;
	std::vector<ObjAttri> mObjAttris;
	std::vector<Buffer> mObjectAttrisBuffer;

	double mDeltaTime = 0;

	vec2 mCursorPos;
	bool mIsRotating = false;

	std::vector<VkFramebuffer> mFrameBuffers;
	std::unique_ptr<ImGUIRenderPass> mRenderPass;
	std::unique_ptr<Image> mDepthImage;

	VkClearValue clearColor = { {{0.0f, 0.5f, 0.7f, 1.0f}} };
	VkClearValue clearDepthStencil = { .depthStencil = {1.0f, 0} };
	std::array<VkClearValue, 2> mClearValues = { clearColor, clearDepthStencil };

	void CreateFrameBuffers();

	VkDescriptorPool mImguiPool;
};

