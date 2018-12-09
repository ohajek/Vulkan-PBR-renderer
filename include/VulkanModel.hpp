#pragma once

#include <vulkan/vulkan.hpp>

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>

namespace vkpbr {
	
	enum class VertexComponent {
		position = 0x0,
		normal = 0x1,
		color = 0x2,
		uv = 0x3,
		tangent = 0x4,
		bitangent = 0x5,
		dummy_float = 0x6,
		dummy_vec4 = 0x7
	};


	struct VertexLayout {
	public:
		std::vector<VertexComponent> components;

		VertexLayout(std::vector<VertexComponent> components)
		{
			this->components = std::move(components);
		}

		uint32_t stride()
		{
			uint32_t res = 0;
			for (auto& component : components) {
				switch (component) {
				case VertexComponent::uv:
					res += 2 * sizeof(float);
					break;
				case VertexComponent::dummy_float:
					res += sizeof(float);
					break;
				case VertexComponent::dummy_vec4:
					res += 4 * sizeof(float);
					break;
				default:
					// All components except the ones listed above are made up of 3 floats
					res += 3 * sizeof(float);
				}
			}
			return res;
		}
	};


	/* Used to parametrize model loading */
	struct ModelCreateInfo {
		glm::vec3 center;
		glm::vec3 scale;
		glm::vec2 uvscale;

		ModelCreateInfo() {};

		ModelCreateInfo(const glm::vec3 scale, const glm::vec2 uvscale, const glm::vec3 center)
		{
			this->center = center;
			this->scale = scale;
			this->uvscale = uvscale;
		}

		ModelCreateInfo(const float scale, const float uvscale, const float center)
		{
			this->center = glm::vec3(center);
			this->scale = glm::vec3(scale);
			this->uvscale = glm::vec2(uvscale);
		}
	};


	struct Model {
		vk::Device device = nullptr;
		vkpbr::Buffer vertices;
		vkpbr::Buffer indices;
		uint32_t indexCount = 0;
		uint32_t vertexCount = 0;

		using ModelPart = struct {
			uint32_t vertexBase;
			uint32_t vertexCount;
			uint32_t indexBase;
			uint32_t indexCount;
		};
		std::vector<ModelPart> parts;

		static const int defaultFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

		using Dimension = struct {
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
		};
		Dimension dimension;

		auto destroy() -> void
		{
			assert(device);
			device.destroyBuffer(vertices.buffer, nullptr);
			device.freeMemory(vertices.memory, nullptr);
			if (indices.buffer) {
				device.destroyBuffer(indices.buffer, nullptr);
				device.freeMemory(indices.memory, nullptr);
			}
		}

		auto loadFromFile(const std::string& filename, const vkpbr::VertexLayout layout, const float scale, vkpbr::VulkanDevice *device, vk::Queue copy_queue, const int flags = defaultFlags) -> bool
		{
			vkpbr::ModelCreateInfo model_create_info(scale, 1.0f, 0.0f);
			return loadFromFile(filename, layout, &model_create_info, device, copy_queue, flags);
		}

		auto loadFromFile(const std::string& filename, vkpbr::VertexLayout layout, vkpbr::ModelCreateInfo* create_info, vkpbr::VulkanDevice* device, vk::Queue copy_queue, const int flags = defaultFlags) -> bool
		{
			this->device = device->logicalDevice;

			Assimp::Importer importer;
			const aiScene* pScene;

			// Load file
			pScene = importer.ReadFile(filename.c_str(), flags);
			if (!pScene) {
				const std::string error = importer.GetErrorString();
				std::cerr << "[ERROR]: Could not load model : " << error << std::endl;
				exit(EXIT_FAILURE);
			}

			if (pScene) {
				parts.clear();
				parts.resize(pScene->mNumMeshes);

				glm::vec3 scale(1.0f);
				glm::vec2 uvscale(1.0f);
				glm::vec3 center(0.0f);
				if (create_info) {
					scale = create_info->scale;
					uvscale = create_info->uvscale;
					center = create_info->center;
				}

				auto vertex_buffer = std::vector<float>{};
				auto index_buffer = std::vector<uint32_t>{};
				vertexCount = 0;
				indexCount = 0;

				// Load meshes
				for (unsigned int i = 0; i < pScene->mNumMeshes; i++) {
					const auto* paiMesh = pScene->mMeshes[i];

					parts[i] = {};
					parts[i].vertexBase = vertexCount;
					parts[i].indexBase = indexCount;

					vertexCount += pScene->mMeshes[i]->mNumVertices;

					aiColor3D model_color(0.0f, 0.0f, 0.0f);
					pScene->mMaterials[paiMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, model_color);

					const aiVector3D zero_vector(0.0f, 0.0f, 0.0f);

					for (uint32_t j = 0; j < paiMesh->mNumVertices; j++) {
					
						const auto* position = &(paiMesh->mVertices[j]);
						const auto* normal = &(paiMesh->mNormals[j]);
						const auto* texcoord = (paiMesh->HasTextureCoords(0)) ? &(paiMesh->mTextureCoords[0][j]) : &zero_vector;
						const auto* tangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mTangents[j]) : &zero_vector;
						const auto* bitangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mBitangents[j]) : &zero_vector;

						for (auto& component : layout.components) {
							
							switch(component) {
							case VertexComponent::position:
								vertex_buffer.push_back(position->x * scale.x + center.x);
								vertex_buffer.push_back(-position->y * scale.y + center.y);
								vertex_buffer.push_back(position->z * scale.z + center.z);
								break;
							case VertexComponent::normal:
								vertex_buffer.push_back(normal->x);
								vertex_buffer.push_back(-normal->y);
								vertex_buffer.push_back(normal->z);
								break;
							case VertexComponent::uv:
								vertex_buffer.push_back(texcoord->x * uvscale.s);
								vertex_buffer.push_back(texcoord->y * uvscale.t);
								break;
							case VertexComponent::color:
								vertex_buffer.push_back(model_color.r);
								vertex_buffer.push_back(model_color.g);
								vertex_buffer.push_back(model_color.b);
								break;
							case VertexComponent::tangent:
								vertex_buffer.push_back(tangent->x);
								vertex_buffer.push_back(tangent->y);
								vertex_buffer.push_back(tangent->z);
								break;
							case VertexComponent::bitangent:
								vertex_buffer.push_back(bitangent->x);
								vertex_buffer.push_back(bitangent->y);
								vertex_buffer.push_back(bitangent->z);
								break;
								/*Padding with dummy components */
							case VertexComponent::dummy_float:
								vertex_buffer.push_back(0.0f);
								break;
							case VertexComponent::dummy_vec4:
								vertex_buffer.push_back(0.0f);
								vertex_buffer.push_back(0.0f);
								vertex_buffer.push_back(0.0f);
								vertex_buffer.push_back(0.0f);
								break;
							};
						}

						dimension.max.x = fmax(position->x, dimension.max.x);
						dimension.max.y = fmax(position->y, dimension.max.y);
						dimension.max.z = fmax(position->z, dimension.max.z);

						dimension.min.x = fmin(position->x, dimension.min.x);
						dimension.min.y = fmin(position->y, dimension.min.y);
						dimension.min.z = fmin(position->z, dimension.min.z);
					}

					dimension.size = dimension.max - dimension.min;
					parts[i].vertexCount = paiMesh->mNumVertices;

					const auto index_base = static_cast<uint32_t>(index_buffer.size());
					
					for (unsigned int j = 0; j < paiMesh->mNumFaces; j++) {
						const auto& face = paiMesh->mFaces[j];
						if (face.mNumIndices != 3) {
							continue;
						}
						index_buffer.push_back(index_base + face.mIndices[0]);
						index_buffer.push_back(index_base + face.mIndices[1]);
						index_buffer.push_back(index_base + face.mIndices[2]);
						parts[i].indexCount += 3;
						indexCount += 3;
					}
				}

				const auto vertex_buffer_size = static_cast<uint32_t>(vertex_buffer.size()) * sizeof(float);
				const auto index_buffer_size = static_cast<uint32_t>(index_buffer.size()) * sizeof(uint32_t);

				/* Using staging buffers to move vertex and index buffer to device local memory */
				vkpbr::Buffer vertex_staging;
				vkpbr::Buffer index_staging;

				/* Vertex buffer */
				device->createBuffer(
					vertex_buffer_size,
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					vertex_staging,
					vertex_buffer.data()
				);

				/* Index buffer */
				device->createBuffer(
					index_buffer_size,
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					index_staging,
					index_buffer.data()
				);

				/* device local target buffers */
				/* Vertex buffer */
				device->createBuffer(
					vertex_buffer_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
					vk::MemoryPropertyFlagBits::eDeviceLocal,
					vertices
				);

				/* Index buffer */
				device->createBuffer(
					index_buffer_size,
					vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
					vk::MemoryPropertyFlagBits::eDeviceLocal,
					indices
				);

				auto copy_command = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

				vk::BufferCopy copy_region = {};
				copy_region.size = vertices.size;
				copy_command.copyBuffer(vertex_staging.buffer, vertices.buffer, 1, &copy_region);

				copy_region.size = indices.size;
				copy_command.copyBuffer(index_staging.buffer, indices.buffer, 1, &copy_region);

				device->finishAndSubmitCmdBuffer(copy_command, copy_queue);

				/* Cleanup */
				device->logicalDevice.destroyBuffer(vertex_staging.buffer, nullptr);
				device->logicalDevice.freeMemory(vertex_staging.memory, nullptr);
				device->logicalDevice.destroyBuffer(index_staging.buffer, nullptr);
				device->logicalDevice.freeMemory(index_staging.memory, nullptr);

				return true;
			}
			else {
				std::cerr << "[ERROR]: Error while parsing " << filename << " : " << importer.GetErrorString();
				return false;
			}
		}
	};
}