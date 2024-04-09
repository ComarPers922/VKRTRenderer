#include "Instance.h"

VkInstance Instance::mDefaultInstance = nullptr;

VkInstance& Instance::GetDefaultVkInstance()
{
	if (mDefaultInstance)
	{
		return mDefaultInstance;
	}
	// ��ʼ��VOLK
	volkInitialize();
	// ��ʼ��Ӧ���йصĲ�����ע�������õ���Vulkan�汾��1.2
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "VKRT";
	appInfo.applicationVersion = VK_API_VERSION_1_2;
	appInfo.pEngineName = "VulkanApp";
	appInfo.engineVersion = VK_API_VERSION_1_2;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	// ��GLFW���������Ҫ��GLFW������Ⱦ��Ҫ��Щ��չ
	uint32_t requiredExtensionsCount = 0;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

	std::vector<const char*> extensions;
	std::vector<const char*> layers;

	// ����Vulkan��У���
	extensions.insert(extensions.begin(), requiredExtensions, requiredExtensions + requiredExtensionsCount);

	extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	layers.push_back("VK_LAYER_KHRONOS_validation");
	// ����VkInstance��Ҫ�Ĳ���
	VkInstanceCreateInfo instInfo;
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext = nullptr;
	instInfo.flags = 0;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instInfo.ppEnabledExtensionNames = extensions.data();
	instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instInfo.ppEnabledLayerNames = layers.data();

	// ����instance������Ƿ񴴽��ɹ�
	VkResult error = vkCreateInstance(&instInfo, nullptr, &mDefaultInstance);
	assert(error == VK_SUCCESS);
	// VOLK�������instance
	volkLoadInstance(mDefaultInstance);

	return mDefaultInstance;
}
