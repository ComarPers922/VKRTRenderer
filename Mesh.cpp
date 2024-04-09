//
// Created by 廖宇航 on 2022-01-12.
//

#include "Mesh.h"

#include <assimp/postprocess.h>
#include <stdexcept>
#include <queue>
#include <glm/gtx/transform.hpp>

#include "VKRTApp.h"

Mesh::Mesh(VkDevice& logicalDevice,
	VkCommandPool& pool,
	VkQueue& graphicsQueue,
	VmaAllocator& allocator,
	const std::vector<vec3>& positions,
	const std::vector<MyVertexAttribute>& vertexAttributes,
	const std::vector<uint32_t>& indices,
	const std::string& matInfo,
	const uint32_t& matID,
	const aiColor4D& color,
	const mat4& transform) :
	mLogicalDevice(logicalDevice),
	positions(positions),
	vertAttributes(vertexAttributes),
	indices(indices),
	positionBuffer(allocator),
	vertAttriBuffer(allocator),
	indexBuffer(allocator),
	facesBuffer(allocator),
	colorBuffer(allocator),
	matInfo(matInfo),
	matID(matID),
	diffuseTex(allocator, logicalDevice),
	transform(transform),
	mCommandPool(pool),
	mGraphicsQueue(graphicsQueue),
	mAllocator(allocator),
	mColor(color)
{
	CHECK_VK_ERROR(positionBuffer.CreateBuffer(sizeof(vec3) * positions.size(),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT), "Failed to create a vertex position buffer.");

	CHECK_VK_ERROR(vertAttriBuffer.CreateBuffer(sizeof(MyVertexAttribute) * vertexAttributes.size(),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT), "Failed to create a vertex attribute buffer.");

	CHECK_VK_ERROR(indexBuffer.CreateBuffer(sizeof(uint32_t) * indices.size(),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT), "Failed to create an index buffer.");

	if (diffuseTex.LoadImageFromFile(matInfo.c_str(), mCommandPool, mGraphicsQueue))
	{
		CHECK_VK_ERROR(diffuseTex.CreateImageView(VK_IMAGE_VIEW_TYPE_2D,
			VkImageSubresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			}), "Failed to load image from file.");
		CHECK_VK_ERROR(diffuseTex.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT),
			"Failed to create a sampler of a texture.");
	}

	auto numFaces = indices.size() / 3;
	faces.resize(numFaces * 4);
	// faces.assign(faces.size(), FACE_NUM);
	for (size_t i = 0; i < numFaces; i ++)
	{
		faces[4 * i + 0] = indices[3 * i + 0];
		faces[4 * i + 1] = indices[3 * i + 1];
		faces[4 * i + 2] = indices[3 * i + 2];
	}

	CHECK_VK_ERROR(facesBuffer.CreateBuffer(sizeof(uint32_t) * faces.size(),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT), "Failed to create an faces buffer.");

	matIDs.resize(numFaces);
	matIDs.assign(matIDs.size(), matID);

	positionBuffer.UploadData(positions.data());
	vertAttriBuffer.UploadData(vertAttributes.data());
	indexBuffer.UploadData(indices.data());
	facesBuffer.UploadData(faces.data());

	colorBuffer.CreateBuffer(sizeof(aiColor4D),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	colorBuffer.UploadData(&mColor);
}

size_t Mesh::GetPositionCount() const
{
	return positions.size();
}

size_t Mesh::GetVertAttributeCount() const
{
	return vertAttributes.size();
}

size_t Mesh::GetIndexCount() const
{
	return indices.size();
}

Buffer& Mesh::GetPositionBuffer()
{
	return positionBuffer;
}

Buffer& Mesh::GetVertAttriBuffer()
{
	return vertAttriBuffer;
}

Buffer& Mesh::GetIndexBuffer()
{
	return indexBuffer;
}

Buffer& Mesh::GetFacesBuffer()
{
	return facesBuffer;
}

Buffer& Mesh::GetColorBuffer()
{
	return colorBuffer;
}

Mesh::~Mesh() = default;

std::shared_ptr<Mesh> Mesh::ImportMeshFromFileOfIndex(VkDevice& logicalDevice,
	VkCommandPool& pool,
	VkQueue& graphicsQueue,
	VmaAllocator& allocator, const std::string& path, size_t index)
{
	Assimp::Importer importer;
	const auto* scene = importer.ReadFile(path, aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices
		| aiProcess_FlipUVs
		| aiProcess_GenNormals);

	return ImportMeshFromAIScene(logicalDevice, pool, graphicsQueue, allocator, scene, index, path, 0);
}

std::vector<std::shared_ptr<Mesh>> Mesh::ImportAllMeshesFromFile(VkDevice& logicalDevice,
	VkCommandPool& pool,
	VkQueue& graphicsQueue,
	VmaAllocator& allocator, const std::string& path)
{
	// 因为导入结果可能有一组模型，所以需要是个Vector
	auto result = std::vector<std::shared_ptr<Mesh>>();
	Assimp::Importer importer;
	// 读取文件，并且将所有顶点三角化、减少顶点数、反转UV的Y轴，最后如果没有Normals的话生成Normals
	const auto* scene = importer.ReadFile(path, aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs | aiProcess_GenNormals);

	if (scene == nullptr)
	{
		return result;
	}

	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		auto curMesh = ImportMeshFromAIScene(logicalDevice, pool, graphicsQueue, allocator, scene, i, path, static_cast<uint32_t>(i));
		if (curMesh != nullptr)
		{
			result.push_back(curMesh);
		}
	}

	// Set Transform
	std::queue<aiNode*> nodeQueue;
	nodeQueue.push(scene->mRootNode);

	while (!nodeQueue.empty())
	{
		const auto* curNode = nodeQueue.front();
		nodeQueue.pop();

		for (size_t i = 0; i < curNode->mNumMeshes; i++)
		{
			const auto& meshIndex = curNode->mMeshes[i];
			if (meshIndex < result.size())
			{
				auto& curTransform = result[meshIndex]->transform;
				auto curSourceTransform = curNode->mTransformation;
				curTransform[0] = vec4(curSourceTransform[0][0], curSourceTransform[1][0], curSourceTransform[2][0], curSourceTransform[3][0]);
				curTransform[1] = vec4(curSourceTransform[0][1], curSourceTransform[1][1], curSourceTransform[2][1], curSourceTransform[3][1]);
				curTransform[2] = vec4(curSourceTransform[0][2], curSourceTransform[1][2], curSourceTransform[2][2], curSourceTransform[3][2]);
				curTransform[3] = vec4(curSourceTransform[0][3], curSourceTransform[1][3], curSourceTransform[2][3], curSourceTransform[3][3]);
				curTransform = glm::transpose(curTransform);
				result[meshIndex]->aiMatrixTransform =
					aiMatrix4x4(aiVector3t{ 1.f, 1.f, 1.f }, aiQuaternion(0, 0, 0), aiVector3t<float> {0, 0, 0})
					* curSourceTransform;
			}
		}
		for (size_t i = 0; i < curNode->mNumChildren; i++)
		{
			nodeQueue.push(curNode->mChildren[i]);
		}
	}

	return result;
}

void Mesh::Dispose()
{
	colorBuffer.Free();
	facesBuffer.Free();
	indexBuffer.Free();
	positionBuffer.Free();
	vertAttriBuffer.Free();

	diffuseTex.Dispose();

	vkDestroyAccelerationStructureKHR(mLogicalDevice, mAccelerationStructure.accelerationStructure, VK_NULL_HANDLE);
	mAccelerationStructure.buffer->Free();
}

std::shared_ptr<Mesh> Mesh::ImportMeshFromAIScene(VkDevice& logicalDevice,
                                                  VkCommandPool& pool,
                                                  VkQueue& graphicsQueue,
                                                  VmaAllocator& allocator,
                                                  const aiScene* scene, size_t index, const std::string& path, const uint32_t& matID)
{
	std::vector<vec3> positions;
	std::vector<MyVertexAttribute> vertexAttributes;
	std::vector<uint32_t> indices;
	std::string matInfo;

	if (scene->mNumMeshes < index)
	{
		return nullptr;
	}

	const auto* mesh = scene->mMeshes[index];

	positions.reserve(mesh->mNumVertices);
	indices.reserve(mesh->mNumFaces * 3);

	for (size_t i = 0; i < mesh->mNumVertices; i++)
	{
		const auto& curVert = mesh->mVertices[i];

		aiVector3t<float> curTexCoord = { 0, 0, 0 };
		auto* meshUVs = mesh->mTextureCoords[0];
		if (meshUVs)
		{
			curTexCoord = mesh->mTextureCoords[0][i];
		}
		const auto& curNormal = mesh->mNormals[i];

		positions.emplace_back(curVert.x, curVert.y, curVert.z);
		vertexAttributes.emplace_back(
			vec4{ curNormal.x, curNormal.y, curNormal.z, static_cast<float>(matID)},
			vec4{ curTexCoord.x, curTexCoord.y, 1.0f, 1.0f });
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		const auto& curFace = mesh->mFaces[i];
		for (size_t j = 0; j < 3; j++)
		{
			indices.emplace_back(curFace.mIndices[j]);
		}
	}
	aiColor4D outColor{};
	if (scene->HasMaterials())
	{
		auto matIndex = mesh->mMaterialIndex;
		auto* curMat = scene->mMaterials[matIndex];
		aiString textureName;
		// ASSIMP加载模型的时候会以类似于字典的结构储存各个材质的信息
		// 并且就算是Diffuse的贴图，也有可能一个材质有多个
		// 所以这里不只是要指定DIFFUSE，还要指定序号
		curMat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), textureName);
		matInfo = std::string(textureName.C_Str());
		// 获取DIFFUSE的颜色
		auto result = curMat->Get(AI_MATKEY_COLOR_DIFFUSE, outColor);
		// 如果没有材质也没有颜色，记录错误信息（小于0代表错误），待会渲染的时候使用(1,0,1)
		if (!matInfo.empty() || result != aiReturn_SUCCESS)
		{
			outColor = { -1.0, -1.0f, -1.0f, -1.0f };
		}
		// 把材质的图片路径转换为textures下的一个路径
		const std::string directory(DEFAULT_TEX_DIR);
		matInfo = directory + matInfo;
	}
	// 用上面的数据构建Mesh
	auto newMesh = std::make_shared<Mesh>(logicalDevice,
		pool,
		graphicsQueue,
		allocator,
		positions, vertexAttributes, indices, matInfo,
		matID,
		outColor);
	// 记录当前Mesh的名称
	auto meshName = std::string(mesh->mName.C_Str());
	if (meshName.find("Window") != std::string::npos)
	{
		newMesh->mMeshType = WINDOW;
	}

	newMesh->mName = meshName;
	return newMesh;
}

void Mesh::SetModel(const mat4& newModel)
{
	modelObj.model = newModel;
}

mat4 Mesh::GetModelMat4() const
{
	return modelObj.model;
}

MeshModelMat4& Mesh::GetModelObj()
{
	return modelObj;
}
