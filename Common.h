#pragma once

#define VMA_VULKAN_VERSION 1002000
#define GLFW_INCLUDE_VULKAN
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <cassert>
#include <functional>
#include <vector>
#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>
#include <stb/stb_image.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#define STATIC_INLINE_GETTER(TYPE, TARGET)\
	static TYPE& Get##TARGET() { return TARGET; }
#define ASSERT_VK(error) assert(error == VK_SUCCESS)
#define CHECK_VK_ERROR(_error, _message)		\
	do{                                          \
    if (VK_SUCCESS != (_error))					\
	{											\
        assert(false && (_message));             \
    }}while(false)

inline bool CheckIsVkSuccess(const VkResult& result)
{
    return result == VK_SUCCESS;
}

#define RETURN_IF_NOT_SUCCESS(VK_RESULT) \
	{ VkResult __result = VK_RESULT; \
	if (!CheckIsVkSuccess(__result)) return __result; } do{}while(false)

using vec2 = glm::highp_vec2;
using vec3 = glm::highp_vec3;
using vec4 = glm::highp_vec4;
using mat4 = glm::highp_mat4;
using quat = glm::highp_quat;

//using vec2 = glm::vec2;
//using vec3 = glm::vec3;
//using vec4 = glm::vec4;
//using mat4 = glm::mat4;
//using quat = glm::quat;

class Buffer;

VkDeviceOrHostAddressKHR GetBufferDeviceAddress(VkDevice& logicalDevice, Buffer& buffer);

VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst(VkDevice& logicalDevice, Buffer& buffer);

uint32_t AlignUp(const uint32_t value, const uint32_t align);

class IDisposable
{
	virtual void Dispose() = 0;
};

struct Recti { int left, top, right, bottom; };

constexpr auto MatProjection = glm::perspectiveRH_ZO<float>;
constexpr auto MatLookAt = glm::lookAtRH<float, glm::highp>;

float Deg2Rad(const float& deg);
quat QAngleAxis(const float angleRad, const vec3& axis);
vec3 QRotate(const quat& q, const vec3& v);

void ImageBarrier(VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageSubresourceRange& subresourceRange,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout);

VkResult DoOneTimeCommand(VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    std::function<VkResult(VkCommandBuffer&)> recordCommandFunc);