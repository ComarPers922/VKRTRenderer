//
// Created by 廖宇航 on 2022-01-12.
//
#pragma once

#include <vector>
#include <memory>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "glm/glm.hpp"
#include "Buffer.h"
#include "Image.h"
#include "Constants.h"

const unsigned char FACE_NUM = 3;

struct MyVertexAttribute
{
	vec4 normal;
	vec4 texCoord;
};

struct MeshModelMat4
{
	mat4 model;
};

struct AccelerationStructure
{
	std::shared_ptr<Buffer> buffer;
	VkAccelerationStructureKHR accelerationStructure;
	VkDeviceAddress handle;
};

enum MeshType
{
	OPAQUE = 0, WINDOW, MESH_TYPE_MAX
};

class Mesh
{
public:
	Mesh(VkDevice& logicalDevice,
		VkCommandPool& pool,
		VkQueue& graphicsQueue,
		VmaAllocator& allocator,
		const std::vector<vec3>& positions,
		const std::vector<MyVertexAttribute>& texCoords,
		const std::vector<uint32_t>& indices,
		const std::string& matInfo,
		const uint32_t& matID,
		const aiColor4D& color = { 1.0f, 1.0f, 1.0f, 1.0f },
		const mat4& transform = mat4(1.0f));

	size_t GetPositionCount() const;
	size_t GetVertAttributeCount() const;
	size_t GetIndexCount() const;

	Buffer& GetPositionBuffer();
	Buffer& GetVertAttriBuffer();
	Buffer& GetIndexBuffer();
	Buffer& GetFacesBuffer();
	Buffer& GetColorBuffer();

	void SetModel(const mat4& newModel);
	[[nodiscard]] mat4 GetModelMat4() const;
	MeshModelMat4& GetModelObj();

	void Dispose();

	virtual ~Mesh();

	static std::shared_ptr<Mesh> ImportMeshFromFileOfIndex(VkDevice& logicalDevice,
		VkCommandPool& pool,
		VkQueue& graphicsQueue,
		VmaAllocator& allocator, const std::string& path, size_t index);
	static std::vector<std::shared_ptr<Mesh>> ImportAllMeshesFromFile(VkDevice& logicalDevice,
		VkCommandPool& pool,
		VkQueue& graphicsQueue,
		VmaAllocator& allocator, const std::string& path);

	[[nodiscard]] const std::vector<vec3>& GetPositions() const
	{
		return positions;
	}

	[[nodiscard]] const std::vector<MyVertexAttribute>& GetVertAttributes() const
	{
		return vertAttributes;
	}

	[[nodiscard]] const std::vector<uint32_t>& GetIndicies() const
	{
		return indices;
	}

	[[nodiscard]] const std::vector<uint32_t>& GetFaces() const
	{
		return faces;
	}

	[[nodiscard]] const std::vector<uint32_t>& GetMatIDs() const
	{
		return matIDs;
	}

	[[nodiscard]] const uint32_t& GetMatID() const
	{
		return matID;
	}

	[[nodiscard]] const Image& GetDiffuseTex() const
	{
		return diffuseTex;
	}

	[[nodiscard]] const std::string& GetMatInfo() const
	{
		return matInfo;
	}

	[[nodiscard]] const mat4& GetTransform() const
	{
		return transform;
	}

	[[nodiscard]] const aiMatrix4x4 GetaiMatrix4x4() const
	{
		return aiMatrixTransform;
	}

	[[nodiscard]] AccelerationStructure& GetAccelerationStructure()
	{
		return mAccelerationStructure;
	}

	[[nodiscard]] const MeshType& GetMeshType() const
	{
		return mMeshType;
	}

	[[nodiscard]] const std::string& GetName() const
	{
		return mName;
	}

protected:
	VkDevice& mLogicalDevice;
	MeshModelMat4 modelObj;

	std::vector<vec3> positions; // 顶点坐标
	std::vector<MyVertexAttribute> vertAttributes; // 顶点描述
	std::vector<uint32_t> indices; // 索引
	std::vector<uint32_t> faces; // 面
	std::vector<uint32_t> matIDs; // 材质ID

	Buffer positionBuffer;
	Buffer vertAttriBuffer;
	Buffer indexBuffer;
	Buffer facesBuffer;
	Buffer colorBuffer;

	std::string matInfo;
	uint32_t matID;

	Image diffuseTex;

	mat4 transform;
	aiMatrix4x4 aiMatrixTransform;

	VkCommandPool& mCommandPool;
	VkQueue& mGraphicsQueue;
	VmaAllocator& mAllocator;

	AccelerationStructure mAccelerationStructure;

	aiColor4D mColor;

	MeshType mMeshType = OPAQUE;

	std::string mName;
private:
	static std::shared_ptr<Mesh> ImportMeshFromAIScene(VkDevice& logicalDevice,
		VkCommandPool& pool,
		VkQueue& graphicsQueue,
		VmaAllocator& allocator,
		const aiScene* scene, size_t index, const std::string& path, const uint32_t& matID);
};

