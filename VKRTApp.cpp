#include "VKRTApp.h"
#include "Device.h"
#include "FileUtility.h"

#include "Instance.h"
#include "shared_with_shaders.h"
#include "DescriptorSet.h"

VKRTApp::VKRTApp(GLFWwindow* window, const uint32_t& width, const uint32_t& height) :
	mWindow(window),
	mWidth(width),
	mHeight(height)
{
	// 每个Vulkan程序都需要一个VKInstance
	InitInstance();

	// 初始化用于Vulkan渲染的设备
	InitDevice();

	// Surface就是展示Vulkan最终绘制的结果的地方
	mSurface = std::make_unique<Surface>(mInstance,
		window,
		Device::GetPhysicalDevice(),
		Device::GetQueue().GraphicsQueueFamilyIndex);

	// 程序使用了AMD的VMA，一个更高效的管理显存与分配Buffer的API
	CHECK_VK_ERROR(InitVma(), "Failed to init VMA.");

	// 交换链
	mSwapchain = std::make_unique<Swapchain>(Device::GetPhysicalDevice(),
		Device::GetLogicalDevice(),
		mSurface->GetSurface(),
		mSurface->GetSurfaceFormat(),
		mWidth, mHeight);

	// 命令池，用来分配CommandBuffer
	CHECK_VK_ERROR(InitCommandPool(), "Failed to init command pool.");

	/*
	 * 光线追踪管线与光栅化不同，需要预先传入一个图片，作为光线追踪管线的画布
	 * 之后每个像素Ray Trace后的结果都会存入这张图片里面
	 */
	CHECK_VK_ERROR(InitializeOffscreenImage(), "Failed to init offscreen image.");
	// 初始化CommandBuffer
	CHECK_VK_ERROR(InitializeCommandBuffers(), "Failed to init command buffers.");

	// 导入模型
	mMeshes = Mesh::ImportAllMeshesFromFile(Device::GetLogicalDevice(),
		mCommandPool,
		Device::GetGraphicsQueue(),
		mVmaAllocator,
		DEFAULT_MODEL_DIR"Loft.obj");

	// 初始化Vulkan绘制同步的相关实例
	CHECK_VK_ERROR(InitializeSynchronization(), "Failed to init synchronization.");

	/*
	 * 创建顶点属性Buffer
	 * 用来传入模型中每个顶点的数据，例如：位置、UV和法线
	 */
	mObjAttris.resize(mMeshes.size());
	mObjectAttrisBuffer.resize(mObjAttris.size(), { mVmaAllocator });
	for (size_t i = 0; i < mObjectAttrisBuffer.size(); i ++)
	{
		auto& curBuffer = mObjectAttrisBuffer[i];
		curBuffer.CreateBuffer(sizeof(ObjAttri),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		curBuffer.UploadData(&mObjAttris[i]);
	}

	/*
	 * 每一个模型都有一个MatID
	 * 用来标记当前模型，在光线追踪管线内如果射线命中，应该是选择哪个贴图来采样
	 */
	mMatIDsBuffers.resize(mMeshes.size());
	for (size_t i = 0; i < mMeshes.size(); i++)
	{
		auto& matIDsBuffer = mMatIDsBuffers[i];

		const auto matIDsBufferSize = mMeshes[i]->GetMatIDs().size() * sizeof(uint32_t);
		matIDsBuffer = std::make_shared<Buffer>(mVmaAllocator);
		matIDsBuffer->CreateBuffer(matIDsBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		matIDsBuffer->UploadData(mMeshes[i]->GetMatIDs().data(), matIDsBufferSize);
	}

	// 天空盒。如果射线没有打中任何物体，就渲染天空盒的材质
	mSkyBoxImage = std::make_unique<Image>(mVmaAllocator, Device::GetLogicalDevice());
	mSkyBoxImage->LoadImageFromFile(DEFAULT_TEXTURE_DIR"Sky_LowPoly_01_Day_a.png",
		mCommandPool,
		Device::GetGraphicsQueue());
	mSkyBoxImage->CreateImageView(
		VK_IMAGE_VIEW_TYPE_2D,
		VkImageSubresourceRange
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS,
		});
	mSkyBoxImage->CreateSampler(
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_REPEAT);

	// 用来给每个模型构建底层加速结构
	mBtmLvlAccStructBuilder = std::make_unique<BottomLevelAccelerationStructureBuilder>(mVmaAllocator);
	mBtmLvlAccStructBuilder->Build(Device::GetLogicalDevice(), mCommandPool, Device::GetGraphicsQueue(), mMeshes);

	/*
	 * 用来构建顶层加速结构
	 * 顶层加速结构是最终会传给Shader的场景
	 */
	mTopLvlAccStruct = std::make_unique<TopLevelAccelerationStructure>(mVmaAllocator);
	mTopLvlAccStruct->Build(Device::GetLogicalDevice(), mCommandPool, Device::GetGraphicsQueue(), mMeshes);

	/*
	 * 包含了当前光线追踪管线所需要的所有Shader
	 * RayGen: 光线追踪的入口，用来发射射线
	 * RayHit：当前射线命中的最近的一个目标
	 * RayMiss：当前射线没有命中任何目标
	 * RayShadowHit：当命中物体后，会朝着光源方向再次发射一条射线。如果这个过程中命中任何其它物体，就说明当前的物体被挡着，需要有个阴影
	 * RayShadowMiss：没有任何其它物体遮挡当前物体，不渲染阴影
	 */
	auto shaderCodeRayGen = FileUtility::readFile(DEFAULT_SHADER_DIR"ray_gen.glsl");
	mRayGen = std::make_shared<ShaderModule>(Device::GetLogicalDevice(), EShLangRayGen, shaderCodeRayGen);

	auto shaderCodeRayHit = FileUtility::readFile(DEFAULT_SHADER_DIR"ray_chit.glsl");
	mRayHit = std::make_shared<ShaderModule>(Device::GetLogicalDevice(), EShLangClosestHit, shaderCodeRayHit);

	auto shaderCodeMiss = FileUtility::readFile(DEFAULT_SHADER_DIR"ray_miss.glsl");
	mRayMiss = std::make_shared<ShaderModule>(Device::GetLogicalDevice(), EShLangMiss, shaderCodeMiss);

	auto shaderShadowHit = FileUtility::readFile(DEFAULT_SHADER_DIR"shadow_ray_chit.glsl");
	mShadowRayHit = std::make_shared<ShaderModule>(Device::GetLogicalDevice(), EShLangClosestHit, shaderShadowHit);

	auto shaderShadowMiss = FileUtility::readFile(DEFAULT_SHADER_DIR"shadow_ray_miss.glsl");
	mShadowRayMiss = std::make_shared<ShaderModule>(Device::GetLogicalDevice(), EShLangMiss, shaderShadowMiss);

	mLayoutRaygen = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_RAYGEN_SET);

#pragma region 初始化Shader会用到的描述集

	// 加速结构
	AddASBinding();
	// 相机
	AddCameraBinding();
	// 光线追踪管线的画布
	AddOffscreenImageBinding();
	// 这个Set是光线追踪光线用到的核心的数据：加速结构、相机和画布
	mLayoutRaygen->CreateDescriptorSet();

	const VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags;
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pNext = nullptr;
	bindingFlags.pBindingFlags = &flag;
	bindingFlags.bindingCount = 1;

	// SSBO: Shader Storage Buffer Object，用来传入Closest Hit Shader里面，需要用来渲染的信息
	VkDescriptorSetLayoutBinding ssboBinding;
	ssboBinding.binding = 0;
	ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboBinding.descriptorCount = mMeshes.size();
	ssboBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	ssboBinding.pImmutableSamplers = nullptr;
	// 每个顶点的材质ID
	mLayoutMaterialIDs = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_MATIDS_SET);
	mLayoutMaterialIDs->AddBinding(ssboBinding);
	mLayoutMaterialIDs->CreateDescriptorSet();
	// 每个顶点的属性，例如：UV、法线等
	mLayoutVertices = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_ATTRIBS_SET);
	mLayoutVertices->AddBinding(ssboBinding);
	mLayoutVertices->CreateDescriptorSet();
	// Faces是IndexBuffer，每三个一组（因为是三角形），组成了一个vec4。不用vec3的原因是为了内存对齐。实际上vec4最后一个分量是没有用到的
	mLayoutFaces = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_FACES_SET);
	mLayoutFaces->AddBinding(ssboBinding);
	mLayoutFaces->CreateDescriptorSet();
	// 传材质
	VkDescriptorSetLayoutBinding textureBinding;
	textureBinding.binding = 0;
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBinding.descriptorCount = mMeshes.size();
	textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	textureBinding.pImmutableSamplers = nullptr;

	mLayoutTexs = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_TEXTURES_SET);
	mLayoutTexs->AddBinding(textureBinding);
	mLayoutTexs->CreateDescriptorSet();
	// 天空盒
	VkDescriptorSetLayoutBinding skyBoxBinding;
	skyBoxBinding.binding = 0;
	skyBoxBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	skyBoxBinding.descriptorCount = 1;
	skyBoxBinding.stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
	skyBoxBinding.pImmutableSamplers = nullptr;

	mLayoutSkyBox = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_ENVS_SET);
	mLayoutSkyBox->AddBinding(skyBoxBinding);
	mLayoutSkyBox->CreateDescriptorSet();
	// 每个模型的自带的颜色。
	VkDescriptorSetLayoutBinding colorBinding;
	colorBinding.binding = 0;
	colorBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	colorBinding.descriptorCount = mMeshes.size();
	colorBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	colorBinding.pImmutableSamplers = nullptr;

	mLayoutColor = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_COLORS_SET);
	mLayoutColor->AddBinding(colorBinding);
	mLayoutColor->CreateDescriptorSet();
	// 每个模型的属性，例如当前模型是否反射还是折射
	VkDescriptorSetLayoutBinding objAttriBinding;
	objAttriBinding.binding = 0;
	objAttriBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objAttriBinding.descriptorCount = mMeshes.size();
	objAttriBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	objAttriBinding.pImmutableSamplers = nullptr;

	mLayoutObjAttris = std::make_unique<DescriptorSetLayout>(Device::GetLogicalDevice(), SWS_OBJ_ATTR_SET);
	mLayoutObjAttris->AddBinding(objAttriBinding);
	mLayoutObjAttris->CreateDescriptorSet();
	// 描述池，用来分配描述集合
	std::vector<VkDescriptorPoolSize> poolSizes({
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },       // top-level AS
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },                    // output image
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },                   // Camera data
		//
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(mMeshes.size()) * 3 },       // per-face material IDs for each mesh
		// vertex attribs for each mesh
		// faces buffer for each mesh
		//
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(mMeshes.size()) },// textures for each material

		//
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },            // environment texture
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },            // object color
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },            // object attribute
		});

	auto layouts = std::vector<DescriptorSetLayout*>{
		mLayoutRaygen.get(),
		mLayoutMaterialIDs.get(),
		mLayoutVertices.get(),
		mLayoutFaces.get(),
		mLayoutTexs.get(),
		mLayoutSkyBox.get(),
		mLayoutColor.get(),
		mLayoutObjAttris.get(),
	};
	mDescriptorSet = std::make_unique<DescriptorSet>(Device::GetLogicalDevice());
	// 目前的Demo没有做任何优化，有多少Mesh就有多少材质。对于没有材质的物体，也会有一个自带材质传入Shader
	const auto numMeshes = static_cast<uint32_t>(mMeshes.size());
	const auto numMaterials = static_cast<uint32_t>(mMeshes.size());

	mDescriptorSet->InitPool(
		layouts,
		poolSizes,
		numMeshes,
		numMaterials,
		{
			1,
			numMeshes,      // per-face material IDs for each mesh
			numMeshes,      // vertex attribs for each mesh
			numMeshes,      // faces buffer for each mesh
			numMaterials,   // textures for each material
			1,              // environment texture
			numMaterials, // Colors for each material
			numMaterials,
		});
#pragma endregion
	//创建光线追踪光线
	CreatePipelineLayout();

	CreateRayTracingPipeline();

	const auto& swapchainExtent = mSwapchain->GetSwapchainExtent();
	//设置相机信息
	mCamera.SetViewport({ 0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight) });
	mCamera.SetViewPlanes(0.2f, 5000.0f);
	mCamera.SetFovY(60.f);
	// mCamera.LookAt(vec3(0.25f, 3.20f, 6.15f), vec3(0.25f, 2.75f, 5.25f));
	// mCamera.LookAt(vec3(0.25f, 7, 6.15f), vec3(0.0f, 0.0f, 0.0f));

	mCamera.SetPosition({ 1.6f, 1.58f, 3.5f });
	mCamera.SetDirection({ -0.48, -0.1f, 0.87f });

	mParams.sunPosAndAmbient = vec4(0.7f, 0.45f, 0.55f, 0.5f);
	mParams.camPos = vec4(mCamera.GetPosition(), 0.0f);
	mParams.camDir = vec4(mCamera.GetDirection(), 0.0f);
	mParams.camUp = vec4(mCamera.GetUp(), 0.0f);
	mParams.camSide = vec4(mCamera.GetSide(), 0.0f);
	mParams.camNearFarFov = vec4(mCamera.GetNearPlane(), mCamera.GetFarPlane(), Deg2Rad(mCamera.GetFovY()), 0.0f);

	mCameraBuffer = std::make_unique<Buffer>(mVmaAllocator);
	mCameraBuffer->CreateBuffer(sizeof(UniformParams),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	mCameraBuffer->UploadData(&mParams, sizeof(UniformParams));
	// 更新Shader里面的描述集合
	UpdateDescriptorSets();
	// 初始化ImGUI
	InitImGUI();
	/*
	 * 光线追踪管线不需要FrameBuffer（因为有个Offscreen Picture）。
	 * 也没有RenderPass……
	 * 但是ImGUI是光栅化管线，所以需要给ImGUI创建一个FrameBuffer和RenderPass
	 */
	CreateFrameBuffers();
}

VKRTApp::~VKRTApp()
{
	// 清理资源
	vkDeviceWaitIdle(Device::GetLogicalDevice());

	mOffscreenImage->Dispose();
	mSkyBoxImage->Dispose();
	mDepthImage->Dispose();

	for (auto& frameBuffer : mFrameBuffers)
	{
		vkDestroyFramebuffer(Device::GetLogicalDevice(), frameBuffer, VK_NULL_HANDLE);
	}

	mRenderPass->Dispose();

	vkDestroyDescriptorPool(Device::GetLogicalDevice(), mImguiPool, VK_NULL_HANDLE);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyPipeline(Device::GetLogicalDevice(), mRTPipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Device::GetLogicalDevice(), mRTPipelineLayout, VK_NULL_HANDLE);

	mCameraBuffer->Free();

	for (auto& attri : mObjectAttrisBuffer)
	{
		attri.Free();
	}

	for (auto& mesh : mMeshes)
	{
		mesh->Dispose();
	}

	for (auto& mat : mMatIDsBuffers)
	{
		mat->Free();
	}

	mTopLvlAccStruct->Dispose();

	mShaderBindingTable->Dispose();

	mLayoutRaygen->Dispose();
	mLayoutMaterialIDs->Dispose();
	mLayoutVertices->Dispose();
	mLayoutFaces->Dispose();
	mLayoutTexs->Dispose();
	mLayoutSkyBox->Dispose();
	mLayoutColor->Dispose();
	mLayoutObjAttris->Dispose();

	mDescriptorSet->Dispose();

	mRayGen->Dispose();
	mRayHit->Dispose();
	mRayMiss->Dispose();
	mShadowRayHit->Dispose();
	mShadowRayMiss->Dispose();

	vkFreeCommandBuffers(Device::GetLogicalDevice(), mCommandPool, mCommandBuffers.size(), mCommandBuffers.data());
	vkDestroyCommandPool(Device::GetLogicalDevice(), mCommandPool, nullptr);

	vkDestroySemaphore(Device::GetLogicalDevice(), mSemaphoreImageAcquired, VK_NULL_HANDLE);
	vkDestroySemaphore(Device::GetLogicalDevice(), mSemaphoreRenderFinished, VK_NULL_HANDLE);
	mSwapchain->Dispose();
	vmaDestroyAllocator(mVmaAllocator);
	vkDeviceWaitIdle(Device::GetLogicalDevice());
	vkDestroyDevice(Device::GetLogicalDevice(), nullptr);
	vkDestroySurfaceKHR(mInstance, mSurface->GetSurface(), nullptr);
	vkDestroyInstance(mInstance, nullptr);
}

void VKRTApp::InitInstance()
{
	mInstance = Instance::GetDefaultVkInstance();
}

void VKRTApp::InitDevice()
{
	Device::Init(mInstance);
}

VkResult VKRTApp::InitVma()
{
	// 需要把一些Vulkan的具体函数指针传进去，如果不清楚这里究竟发生了什么，请参见：VOLK那一小节
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
	// 指定当前Vulkan的版本与设备信息
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocatorCreateInfo.physicalDevice = Device::GetPhysicalDevice();
	allocatorCreateInfo.device = Device::GetLogicalDevice();
	allocatorCreateInfo.instance = Instance::GetDefaultVkInstance();
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
	// 创建Allocator
	return vmaCreateAllocator(&allocatorCreateInfo, &mVmaAllocator);
}

VkResult VKRTApp::InitCommandPool()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = Device::GetQueue().GraphicsQueueFamilyIndex;

	const VkResult error = vkCreateCommandPool(Device::GetLogicalDevice(),
		&commandPoolCreateInfo,
		nullptr,
		&mCommandPool);
	return error;
}

VkResult VKRTApp::InitializeOffscreenImage()
{
	// 创建一个与当前绘图区域大小相当的图片
	const VkExtent3D extent = { mWidth, mHeight, 1 };
	mOffscreenImage = std::make_unique<Image>(mVmaAllocator, Device::GetLogicalDevice());
	VkResult error = mOffscreenImage->Create(VK_IMAGE_TYPE_2D,
		extent,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VK_SUCCESS != error)
	{
		return error;
	}

	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	error = mOffscreenImage->CreateImageView(VK_IMAGE_VIEW_TYPE_2D, range);
	return error;
}

VkResult VKRTApp::InitializeCommandBuffers()
{
	// 注意这里是CommandBuffer的Vector数组
	// 一开始要依据交换链图片的数量预先给每个CommandBuffer分配好空间
	mCommandBuffers.resize(mSwapchain->GetImageCount());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	// Command Buffer需要从CommandPool里面分配
	commandBufferAllocateInfo.commandPool = mCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	// 指定需要创建的数量
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

	return vkAllocateCommandBuffers(Device::GetLogicalDevice(), &commandBufferAllocateInfo, mCommandBuffers.data());
}

VkResult VKRTApp::InitializeSynchronization()
{
	VkSemaphoreCreateInfo semaphoreCreatInfo;
	semaphoreCreatInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreatInfo.pNext = nullptr;
	semaphoreCreatInfo.flags = 0;

	VkResult error = vkCreateSemaphore(Device::GetLogicalDevice(), &semaphoreCreatInfo, nullptr, &mSemaphoreImageAcquired);
	if (VK_SUCCESS != error)
	{
		return error;
	}

	return vkCreateSemaphore(Device::GetLogicalDevice(), &semaphoreCreatInfo, nullptr, &mSemaphoreRenderFinished);
}

void VKRTApp::AddASBinding()
{
	// 加速结构的绑定
	VkDescriptorSetLayoutBinding accelerationStructureBinding = {};
	accelerationStructureBinding.binding = SWS_SCENE_AS_BINDING;
	accelerationStructureBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureBinding.descriptorCount = 1;
	accelerationStructureBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	accelerationStructureBinding.pImmutableSamplers = VK_NULL_HANDLE;

	mLayoutRaygen->AddBinding(accelerationStructureBinding);
}

void VKRTApp::AddOffscreenImageBinding()
{
	// 画布的绑定
	VkDescriptorSetLayoutBinding offscreenImageBinding = {};
	offscreenImageBinding.binding = SWS_RESULT_IMAGE_BINDING;
	offscreenImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	offscreenImageBinding.descriptorCount = 1;
	offscreenImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	offscreenImageBinding.pImmutableSamplers = VK_NULL_HANDLE;

	mLayoutRaygen->AddBinding(offscreenImageBinding);
}

void VKRTApp::AddCameraBinding()
{
	VkDescriptorSetLayoutBinding cameraBinding = {};
	cameraBinding.binding = SWS_CAMDATA_BINDING;
	cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraBinding.descriptorCount = 1;
	cameraBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	cameraBinding.pImmutableSamplers = VK_NULL_HANDLE;

	mLayoutRaygen->AddBinding(cameraBinding);
}

void VKRTApp::CreatePipelineLayout()
{
	// 创建管线布局
	const auto setLayouts = std::vector<VkDescriptorSetLayout>
	{
		mLayoutRaygen->GetSetLayout(),
		mLayoutMaterialIDs->GetSetLayout(),
		mLayoutVertices->GetSetLayout(),
		mLayoutFaces->GetSetLayout(),
		mLayoutTexs->GetSetLayout(),
		mLayoutSkyBox->GetSetLayout(),
		mLayoutColor->GetSetLayout(),
		mLayoutObjAttris->GetSetLayout(),
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	// pipelineLayoutCreateInfo.setLayoutCount = SWS_NUM_SETS_NO_SKYBOX;
	pipelineLayoutCreateInfo.setLayoutCount = SWS_NUM_SETS;
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();

	const auto error = vkCreatePipelineLayout(Device::GetLogicalDevice(), &pipelineLayoutCreateInfo, nullptr, &mRTPipelineLayout);
	assert(error == VK_SUCCESS);
}

void VKRTApp::CreateRayTracingPipeline()
{
	// 创建光线追踪管线
	const auto& rtProps = Device::GetRTProps();
	mShaderBindingTable = std::make_unique<ShaderBindingTable>(mVmaAllocator);
	mShaderBindingTable->Initialize(2, 2,
		rtProps.shaderGroupHandleSize,
		rtProps.shaderGroupBaseAlignment);

	mShaderBindingTable->SetRaygenStage(mRayGen->GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR));

	mShaderBindingTable->AddStageToHitGroup({ mRayHit->GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, SWS_PRIMARY_HIT_SHADERS_IDX);
	mShaderBindingTable->AddStageToHitGroup({ mShadowRayHit->GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, SWS_SHADOW_HIT_SHADERS_IDX);

	mShaderBindingTable->AddStageToMissGroup({ mRayMiss->GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR) }, SWS_PRIMARY_MISS_SHADERS_IDX);
	mShaderBindingTable->AddStageToMissGroup({ mShadowRayMiss->GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR) }, SWS_SHADOW_MISS_SHADERS_IDX);

	VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
	rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayPipelineInfo.stageCount = mShaderBindingTable->GetNumStages();
	rayPipelineInfo.pStages = mShaderBindingTable->GetStages();
	rayPipelineInfo.groupCount = mShaderBindingTable->GetNumGroups();
	rayPipelineInfo.pGroups = mShaderBindingTable->GetGroups();
	rayPipelineInfo.maxPipelineRayRecursionDepth = SWS_MAX_RECURSION;
	rayPipelineInfo.layout = mRTPipelineLayout;

	const auto error = vkCreateRayTracingPipelinesKHR(
		Device::GetLogicalDevice(),
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		1,
		&rayPipelineInfo,
		VK_NULL_HANDLE,
		&mRTPipeline);
	assert(error == VK_SUCCESS);

	mShaderBindingTable->CreateSBT(Device::GetLogicalDevice(), mRTPipeline);
}

void VKRTApp::UpdateDescriptorSets()
{
	// 往Shader里面实际传数据
	const uint32_t numMeshes = mMeshes.size();
	const uint32_t numMaterials = mMeshes.size();
	auto& mRTDescriptorSets = mDescriptorSet->GetDescriptorSets();

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.pNext = nullptr;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &mTopLvlAccStruct->GetAccelerationStructure().accelerationStructure;

	VkWriteDescriptorSet accelerationStructureWrite;
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
	accelerationStructureWrite.dstSet = mRTDescriptorSets[SWS_SCENE_AS_SET];
	accelerationStructureWrite.dstBinding = SWS_SCENE_AS_BINDING;
	accelerationStructureWrite.dstArrayElement = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureWrite.pImageInfo = nullptr;
	accelerationStructureWrite.pBufferInfo = nullptr;
	accelerationStructureWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	VkDescriptorImageInfo descriptorOutputImageInfo;
	descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
	descriptorOutputImageInfo.imageView = mOffscreenImage->GetImageView();
	descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet resultImageWrite;
	resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	resultImageWrite.pNext = nullptr;
	resultImageWrite.dstSet = mRTDescriptorSets[SWS_RESULT_IMAGE_SET];
	resultImageWrite.dstBinding = SWS_RESULT_IMAGE_BINDING;
	resultImageWrite.dstArrayElement = 0;
	resultImageWrite.descriptorCount = 1;
	resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
	resultImageWrite.pBufferInfo = nullptr;
	resultImageWrite.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo camdataBufferInfo;
	camdataBufferInfo.buffer = mCameraBuffer->GetVkBuffer();
	camdataBufferInfo.offset = 0;
	camdataBufferInfo.range = mCameraBuffer->GetSize();

	VkWriteDescriptorSet camdataBufferWrite;
	camdataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	camdataBufferWrite.pNext = nullptr;
	camdataBufferWrite.dstSet = mRTDescriptorSets[SWS_CAMDATA_SET];
	camdataBufferWrite.dstBinding = SWS_CAMDATA_BINDING;
	camdataBufferWrite.dstArrayElement = 0;
	camdataBufferWrite.descriptorCount = 1;
	camdataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	camdataBufferWrite.pImageInfo = nullptr;
	camdataBufferWrite.pBufferInfo = &camdataBufferInfo;
	camdataBufferWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	std::vector<VkDescriptorBufferInfo> matIDsBufferInfo(numMeshes);
	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& curInfo = matIDsBufferInfo[i];
		curInfo.offset = 0;
		curInfo.buffer = mMatIDsBuffers[i]->GetVkBuffer();
		curInfo.range = mMatIDsBuffers[i]->GetSize();
	}

	VkWriteDescriptorSet matIDsBufferWrite;
	matIDsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	matIDsBufferWrite.pNext = nullptr;
	matIDsBufferWrite.dstSet = mRTDescriptorSets[SWS_MATIDS_SET];
	matIDsBufferWrite.dstBinding = 0;
	matIDsBufferWrite.dstArrayElement = 0;
	matIDsBufferWrite.descriptorCount = numMeshes;
	matIDsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	matIDsBufferWrite.pImageInfo = nullptr;
	matIDsBufferWrite.pBufferInfo = matIDsBufferInfo.data();
	matIDsBufferWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	std::vector<VkDescriptorBufferInfo> vertAttriBufferInfo(numMeshes);
	for (size_t i = 0; i < numMeshes; i++)
	{
		auto& curInfo = vertAttriBufferInfo[i];
		curInfo.offset = 0;
		curInfo.buffer = mMeshes[i]->GetVertAttriBuffer().GetVkBuffer();
		curInfo.range = mMeshes[i]->GetVertAttriBuffer().GetSize();
	}

	VkWriteDescriptorSet attribsBufferWrite;
	attribsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	attribsBufferWrite.pNext = nullptr;
	attribsBufferWrite.dstSet = mRTDescriptorSets[SWS_ATTRIBS_SET];
	attribsBufferWrite.dstBinding = 0;
	attribsBufferWrite.dstArrayElement = 0;
	attribsBufferWrite.descriptorCount = numMeshes;
	attribsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	attribsBufferWrite.pImageInfo = nullptr;
	attribsBufferWrite.pBufferInfo = vertAttriBufferInfo.data();
	attribsBufferWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////
	std::vector<VkDescriptorBufferInfo> facesBufferInfos(numMeshes);
	for (size_t i = 0; i < numMeshes; i++)
	{
		facesBufferInfos[i] =
		{
			mMeshes[i]->GetFacesBuffer().GetVkBuffer(),
			0,
			mMeshes[i]->GetFacesBuffer().GetSize(),
		};
	}

	VkWriteDescriptorSet facesBufferWrite;
	facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	facesBufferWrite.pNext = nullptr;
	facesBufferWrite.dstSet = mRTDescriptorSets[SWS_FACES_SET];
	facesBufferWrite.dstBinding = 0;
	facesBufferWrite.dstArrayElement = 0;
	facesBufferWrite.descriptorCount = numMeshes;
	facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	facesBufferWrite.pImageInfo = nullptr;
	facesBufferWrite.pBufferInfo = facesBufferInfos.data();
	facesBufferWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	std::vector<VkDescriptorImageInfo> textureInfos(numMaterials);

	for (int i = 0; i < numMaterials; i++)
	{
		auto& diffuseImage = mMeshes[i]->GetDiffuseTex();

		textureInfos[i] =
		{
			diffuseImage.GetSampler(),
			diffuseImage.GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
	}

	VkWriteDescriptorSet texturesBufferWrite;
	texturesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesBufferWrite.pNext = nullptr;
	texturesBufferWrite.dstSet = mRTDescriptorSets[SWS_TEXTURES_SET];
	texturesBufferWrite.dstBinding = 0;
	texturesBufferWrite.dstArrayElement = 0;
	texturesBufferWrite.descriptorCount = numMaterials;
	texturesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesBufferWrite.pImageInfo = textureInfos.data();
	texturesBufferWrite.pBufferInfo = nullptr;
	texturesBufferWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	VkDescriptorImageInfo envImageInfo =
	{
		mSkyBoxImage->GetSampler(),
		mSkyBoxImage->GetImageView(),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet envTexturesWrite;
	envTexturesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	envTexturesWrite.pNext = nullptr;
	envTexturesWrite.dstSet = mRTDescriptorSets[SWS_ENVS_SET];
	envTexturesWrite.dstBinding = 0;
	envTexturesWrite.dstArrayElement = 0;
	envTexturesWrite.descriptorCount = 1;
	envTexturesWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	envTexturesWrite.pImageInfo = &envImageInfo;
	envTexturesWrite.pBufferInfo = nullptr;
	envTexturesWrite.pTexelBufferView = nullptr;

	/////////////////////////////////////////////////////////////

	std::vector<VkDescriptorBufferInfo> colorInfos(numMaterials);

	for (int i = 0; i < numMaterials; i++)
	{
		auto& colorBuffer = mMeshes[i]->GetColorBuffer();

		colorInfos[i] =
		{
			colorBuffer.GetVkBuffer(),
			0,
			colorBuffer.GetSize(),
		};
	}

	VkWriteDescriptorSet colorsBufferWrite;
	colorsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	colorsBufferWrite.pNext = nullptr;
	colorsBufferWrite.dstSet = mRTDescriptorSets[SWS_COLORS_SET];
	colorsBufferWrite.dstBinding = 0;
	colorsBufferWrite.dstArrayElement = 0;
	colorsBufferWrite.descriptorCount = numMaterials;
	colorsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	colorsBufferWrite.pImageInfo = nullptr;
	colorsBufferWrite.pBufferInfo = colorInfos.data();
	colorsBufferWrite.pTexelBufferView = nullptr;

	std::vector<VkDescriptorBufferInfo> objAttrisInfos(mObjAttris.size());

	for (int i = 0; i < mObjectAttrisBuffer.size(); i++)
	{
		auto& curBuffer = mObjectAttrisBuffer[i];

		objAttrisInfos[i] =
		{
			curBuffer.GetVkBuffer(),
			0,
			curBuffer.GetSize(),
		};
	}

	VkWriteDescriptorSet objAttrisBufferWrite;
	objAttrisBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	objAttrisBufferWrite.pNext = nullptr;
	objAttrisBufferWrite.dstSet = mRTDescriptorSets[SWS_OBJ_ATTR_SET];
	objAttrisBufferWrite.dstBinding = 0;
	objAttrisBufferWrite.dstArrayElement = 0;
	objAttrisBufferWrite.descriptorCount = objAttrisInfos.size();
	objAttrisBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objAttrisBufferWrite.pImageInfo = nullptr;
	objAttrisBufferWrite.pBufferInfo = objAttrisInfos.data();
	objAttrisBufferWrite.pTexelBufferView = nullptr;

	std::vector<VkWriteDescriptorSet> descriptorWrites({
		accelerationStructureWrite,
		resultImageWrite,
		camdataBufferWrite,
		//
		matIDsBufferWrite,
		//
		attribsBufferWrite,
		//
		facesBufferWrite,
		//
		texturesBufferWrite,
		//
		envTexturesWrite,
		//
		colorsBufferWrite,
		objAttrisBufferWrite,
		});

	vkUpdateDescriptorSets(Device::GetLogicalDevice(),
		static_cast<uint32_t>(descriptorWrites.size()),
		descriptorWrites.data(),
		0,
		VK_NULL_HANDLE);
}

#pragma region 构建渲染的指令
void VKRTApp::FillCommandBuffers()
{
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	for (size_t i = 0; i < mCommandBuffers.size(); i++)
	{
		const VkCommandBuffer commandBuffer = mCommandBuffers[i];

		VkResult error = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		CHECK_VK_ERROR(error, "vkBeginCommandBuffer");

		ImageBarrier(commandBuffer,
			mOffscreenImage->GetImage(),
			subresourceRange,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL);

		this->FillCommandBuffer(commandBuffer, i); // user draw code

		ImageBarrier(commandBuffer,
			mSwapchain->GetImage(i),
			subresourceRange,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		ImageBarrier(commandBuffer,
			mOffscreenImage->GetImage(),
			subresourceRange,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageCopy copyRegion;
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { mWidth, mHeight, 1 };
		vkCmdCopyImage(commandBuffer,
			mOffscreenImage->GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mSwapchain->GetImage(i),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		ImageBarrier(commandBuffer,
			mSwapchain->GetImage(i), subresourceRange,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkRenderPassBeginInfo renderPassBeginInfo
		{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = mRenderPass->GetVkRenderPass(),
				.framebuffer = mFrameBuffers[i],
				.renderArea = {
						.offset = {0, 0},
						.extent = {mWidth, mHeight},
				},
				.clearValueCount = static_cast<uint32_t>(mClearValues.size()),
				.pClearValues = mClearValues.data(),
		};

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
		vkCmdEndRenderPass(commandBuffer);
		error = vkEndCommandBuffer(commandBuffer);
		CHECK_VK_ERROR(error, "vkEndCommandBuffer");
	}
}

void VKRTApp::FillCommandBuffer(VkCommandBuffer commandBuffer, const size_t& imageIndex)
{
	vkCmdBindPipeline(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		mRTPipeline);

	const auto& rtDescriptorSets = mDescriptorSet->GetDescriptorSets();

	vkCmdBindDescriptorSets(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		mRTPipelineLayout, 0,
		static_cast<uint32_t>(rtDescriptorSets.size()), rtDescriptorSets.data(),
		0, 0);

	VkStridedDeviceAddressRegionKHR raygenRegion = {
		mShaderBindingTable->GetSBTAddress() + mShaderBindingTable->GetRaygenOffset(),
		mShaderBindingTable->GetGroupsStride(),
		mShaderBindingTable->GetRaygenSize()
	};

	VkStridedDeviceAddressRegionKHR missRegion = {
		mShaderBindingTable->GetSBTAddress() + mShaderBindingTable->GetMissGroupsOffset(),
		mShaderBindingTable->GetGroupsStride(),
		mShaderBindingTable->GetMissGroupsSize()
	};

	VkStridedDeviceAddressRegionKHR hitRegion = {
		mShaderBindingTable->GetSBTAddress() + mShaderBindingTable->GetHitGroupsOffset(),
		mShaderBindingTable->GetGroupsStride(),
		mShaderBindingTable->GetHitGroupsSize()
	};

	VkStridedDeviceAddressRegionKHR callableRegion = {};

	vkCmdTraceRaysKHR(commandBuffer,
		&raygenRegion,
		&missRegion,
		&hitRegion,
		&callableRegion,
		mWidth, mHeight,
		1u);
}

#pragma endregion

void VKRTApp::InitImGUI()
{
	// 初始化IMGUI
	// VK_NO_PROTOTYPES must be added to the pre-processor define!
	mDepthImage = std::make_unique<Image>(mVmaAllocator, Device::GetLogicalDevice(), VK_FORMAT_D32_SFLOAT);
	mDepthImage->Create(
		VK_IMAGE_TYPE_2D,
		{ mWidth, mHeight, 1 },
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	mDepthImage->CreateImageView(VK_IMAGE_VIEW_TYPE_2D,
		{
			VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1,
		});

	mRenderPass = std::make_unique<ImGUIRenderPass>(Device::GetLogicalDevice());

	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	vkCreateDescriptorPool(Device::GetLogicalDevice(), &pool_info, nullptr, &mImguiPool);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsLight();

	ImGui_ImplGlfw_InitForVulkan(mWindow, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = mInstance;
	init_info.PhysicalDevice = Device::GetPhysicalDevice();
	init_info.Device = Device::GetLogicalDevice();
	init_info.QueueFamily = Device::GetQueue().GraphicsQueueFamilyIndex;
	init_info.Queue = Device::GetGraphicsQueue();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = mImguiPool;
	init_info.Subpass = 0;
	init_info.MinImageCount = mSwapchain->GetImageCount();
	init_info.ImageCount = mSwapchain->GetImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.CheckVkResultFn = [](VkResult err)
		{
			if (err == 0)
				return;
			fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
			if (err < 0)
				abort();
		};
	ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
		return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
		}, &mInstance);
	ImGui_ImplVulkan_Init(&init_info, mRenderPass->GetVkRenderPass());

	DoOneTimeCommand(Device::GetLogicalDevice(), mCommandPool, Device::GetGraphicsQueue(),
		[&](auto& commandBuffer) -> auto
		{
			if (ImGui_ImplVulkan_CreateFontsTexture(commandBuffer))
			{
				return VK_SUCCESS;
			}
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		});

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void VKRTApp::CreateFrameBuffers()
{
	// 为ImGUI创建FrameBuffer
	// 实际上是Post Processing。也就是在刚刚光线追踪管线画好的图片上面画UI
	mFrameBuffers.resize(mSwapchain->GetImageCount());

	for (size_t i = 0; i < mSwapchain->GetImageCount(); i++)
	{
		std::array<VkImageView, 2> attachments =
		{
			mSwapchain->GetImageView(i),
			mDepthImage->GetImageView(),
		};

		VkFramebufferCreateInfo framebufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = mRenderPass->GetVkRenderPass(),
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = mWidth,
			.height = mHeight,
			.layers = 1,
		};
		const auto result = vkCreateFramebuffer(Device::GetLogicalDevice(), &framebufferCreateInfo, nullptr, &mFrameBuffers[i]);
		assert(result == VK_SUCCESS);
	}
}

void VKRTApp::ProcessFrame(const double& deltaTime)
{
	// 处理每一帧的动作
	vkDeviceWaitIdle(Device::GetLogicalDevice());
	const auto& cameraPos = mCamera.GetPosition();
	const auto& cameraRot = mCamera.GetDirection();
	ImGui::Text("Camera Position: (%.3f, %.3f, %.3f)", cameraPos.x, cameraPos.y, cameraPos.z);
	ImGui::Text("Camera Direction: (%.3f, %.3f, %.3f)", cameraRot.x, cameraRot.y, cameraRot.z);

	float sunPos[3] = { mParams.sunPosAndAmbient.x, mParams.sunPosAndAmbient.y, mParams.sunPosAndAmbient.z };
	if (ImGui::SliderFloat3("Directional Light Rotation", sunPos, -1, 1)
		|| ImGui::SliderFloat("Ambient", &mParams.sunPosAndAmbient.w, 0, 2))
	{
		mParams.sunPosAndAmbient.x = sunPos[0];
		mParams.sunPosAndAmbient.y = sunPos[1];
		mParams.sunPosAndAmbient.z = sunPos[2];
		mCameraBuffer->UploadData(&mParams);
	}

	size_t index = 0;

	for (const auto& mesh : mMeshes)
	{
		ImGui::Text("%lld: %s", index, mesh->GetName().c_str());

		ImGui::PushID(&mObjAttris[index]);
		ImGui::SameLine();
		auto changed = ImGui::Checkbox("Reflection", &mObjAttris[index].reflection);
		ImGui::SameLine();
		changed |= ImGui::Checkbox("Refraction", &mObjAttris[index].refraction);
		ImGui::PopID();

		if (changed)
		{
			mObjectAttrisBuffer[index].UploadData(&mObjAttris[index]);
		}

		index++;
	}

	ImGui::Render();

	FillCommandBuffers();

	mDeltaTime = deltaTime;

	uint32_t imageIndex;
	VkResult error = vkAcquireNextImageKHR(Device::GetLogicalDevice(),
		mSwapchain->GetSwapchain(),
		UINT64_MAX,
		mSemaphoreImageAcquired,
		VK_NULL_HANDLE, &imageIndex);
	if (VK_SUCCESS != error) {
		return;
	}

	const VkFence& fence = mSwapchain->GetFence(imageIndex);
	error = vkWaitForFences(Device::GetLogicalDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
	if (VK_SUCCESS != error) {
		return;
	}
	vkResetFences(Device::GetLogicalDevice(), 1, &fence);

	// this->Update(imageIndex, dt);

	const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &mSemaphoreImageAcquired;
	submitInfo.pWaitDstStageMask = &waitStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &mSemaphoreRenderFinished;

	error = vkQueueSubmit(Device::GetGraphicsQueue(), 1, &submitInfo, fence);
	if (VK_SUCCESS != error) {
		return;
	}

	auto swapchain = mSwapchain->GetSwapchain();

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &mSemaphoreRenderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	error = vkQueuePresentKHR(Device::GetGraphicsQueue(), &presentInfo);
	if (VK_SUCCESS != error) {
		return;
	}
	vkDeviceWaitIdle(Device::GetLogicalDevice());
}

void VKRTApp::MoveCamera(const float& side, const float& forward)
{
	mCamera.Move(side * static_cast<float>(mDeltaTime), forward * static_cast<float>(mDeltaTime));
	UpdateCameraBuffer();
}

void VKRTApp::MoveCameraUpDown(const float& amount)
{
	mCamera.SetPosition(mCamera.GetPosition() + vec3(0, 1, 0) * amount * static_cast<float>(mDeltaTime));
	UpdateCameraBuffer();
}

void VKRTApp::RotateCamera(const float& x, const float& y)
{
	vec2 newPos(x, y);
	const float ROTATE_SPEED = 100;

	if (mIsRotating)
	{
		vec2 delta = mCursorPos - newPos;
		mCamera.Rotate(
			delta.x * ROTATE_SPEED * static_cast<float>(mDeltaTime),
			delta.y * ROTATE_SPEED * static_cast<float>(mDeltaTime));
		UpdateCameraBuffer();
	}

	mCursorPos = newPos;
}

void VKRTApp::UpdateCameraBuffer()
{
	mParams.camPos = vec4(mCamera.GetPosition(), 0.0f);
	mParams.camDir = vec4(mCamera.GetDirection(), 0.0f);
	mParams.camUp = vec4(mCamera.GetUp(), 0.0f);
	mParams.camSide = vec4(mCamera.GetSide(), 0.0f);
	mParams.camNearFarFov = vec4(mCamera.GetNearPlane(), mCamera.GetFarPlane(), Deg2Rad(mCamera.GetFovY()), 0.0f);

	mCameraBuffer->UploadData(&mParams, sizeof(UniformParams));
}