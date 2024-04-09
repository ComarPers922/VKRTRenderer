#include "Instance.h"

VkInstance Instance::mDefaultInstance = nullptr;

VkInstance& Instance::GetDefaultVkInstance()
{
	if (mDefaultInstance)
	{
		return mDefaultInstance;
	}
	// 初始化VOLK
	volkInitialize();
	// 初始化应用有关的参数，注意这里用到的Vulkan版本是1.2
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "VKRT";
	appInfo.applicationVersion = VK_API_VERSION_1_2;
	appInfo.pEngineName = "VulkanApp";
	appInfo.engineVersion = VK_API_VERSION_1_2;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	// 向GLFW请求如果需要在GLFW上面渲染需要哪些扩展
	uint32_t requiredExtensionsCount = 0;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

	std::vector<const char*> extensions;
	std::vector<const char*> layers;

	// 加载Vulkan的校验层
	extensions.insert(extensions.begin(), requiredExtensions, requiredExtensions + requiredExtensionsCount);

	extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	layers.push_back("VK_LAYER_KHRONOS_validation");
	// 创建VkInstance需要的参数
	VkInstanceCreateInfo instInfo;
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext = nullptr;
	instInfo.flags = 0;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instInfo.ppEnabledExtensionNames = extensions.data();
	instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instInfo.ppEnabledLayerNames = layers.data();

	// 创建instance并检查是否创建成功
	VkResult error = vkCreateInstance(&instInfo, nullptr, &mDefaultInstance);
	assert(error == VK_SUCCESS);
	// VOLK加载相关instance
	volkLoadInstance(mDefaultInstance);

	return mDefaultInstance;
}
