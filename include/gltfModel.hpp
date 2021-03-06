#pragma once

#include <vulkan/vulkan.hpp>
#include <VulkanDevice.hpp>
#include <VulkanTexture.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny_gltf.h"



namespace vkpbr {
	
	namespace gltf {

		struct Node;

		struct Material {
			enum class AlphaMode { opaque, mask, blend };

			AlphaMode alphaMode = AlphaMode::opaque;
			float     alphaCutoff = 1.0f;
			float     metallicFactor = 1.0f;
			float     roughnessFactor = 1.0f;

			glm::vec4 baseColorFactor = glm::vec4(1.0f);
			glm::vec4 emissiveFactor = glm::vec4(1.0f);

			vkpbr::TextureGLTF* baseColorTexture;
			vkpbr::TextureGLTF* metallicRoughnessTexture;
			vkpbr::TextureGLTF* normalTexture;
			vkpbr::TextureGLTF* occlusionTexture;
			vkpbr::TextureGLTF* emissiveTexture;

			using Extension = struct
			{
				glm::vec4           diffuseFactor = glm::vec4(1.0f);
				glm::vec3           specularFactor = glm::vec3(1.0f);
				vkpbr::TextureGLTF* specularGlossinessTexture;
				vkpbr::TextureGLTF* diffuseTexture;
			};
			Extension extension;

			using PBRWorkflows = struct
			{
				bool metallicRoughness = true;
				bool specularGlossiness = false;
			};
			PBRWorkflows pbrWorkflows;

			vk::DescriptorSet descriptorSet = nullptr;
		};

		struct Primitive {
			uint32_t  firstIndex;
			uint32_t  indexCount;
			Material& material;

			using Dimensions = struct
			{
				glm::vec3 min = glm::vec3(FLT_MAX);
				glm::vec3 max = glm::vec3(-FLT_MAX);
				glm::vec3 size;
				glm::vec3 center;
				float     radius;
			};
			Dimensions dimensions;

			auto setDimensions(glm::vec3 min, glm::vec3 max)
			{
				dimensions.min = min;
				dimensions.max = max;
				dimensions.size = max - min;
				dimensions.center = (min + max) / 2.0f;
				dimensions.radius = glm::distance(min, max) / 2.0f;
			}

			Primitive(uint32_t first_index, uint32_t index_count, Material& material)
				: firstIndex(first_index), indexCount(index_count), material(material) {};
		};

		struct Mesh {
			vkpbr::VulkanDevice*    device;
			std::vector<Primitive*> primitives;

			using UniformBuffer = struct
			{
				vk::Buffer               buffer;
				vk::DeviceMemory         memory;
				vk::DescriptorBufferInfo descriptor;
				vk::DescriptorSet        descriptorSet;
				void*                    mapped;
			};
			UniformBuffer uniformBuffer;

			using UniformBlock = struct
			{
				glm::mat4 matrix;
				glm::mat4 jointMatrix[64]{};
				float     jointCount{0};
			};
			UniformBlock uniformBlock;

			Mesh(vkpbr::VulkanDevice* device, glm::mat4 matrix)
			{
				this->device = device;
				uniformBlock.matrix = matrix;

				VK_ASSERT(
					device->createBuffer(
						sizeof(uniformBlock),
						vk::BufferUsageFlagBits::eUniformBuffer,
						vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
						uniformBuffer.buffer,
						uniformBuffer.memory,
						&uniformBlock
					)
				);
				uniformBuffer.descriptor = vk::DescriptorBufferInfo{ uniformBuffer.buffer, 0, sizeof(uniformBlock) };
			}

			~Mesh()
			{
				device->logicalDevice.destroyBuffer(uniformBuffer.buffer, nullptr);
				device->logicalDevice.freeMemory(uniformBuffer.memory, nullptr);
			}
		};
	
		struct Node {
			Node*              parent;
			uint32_t           index;
			std::vector<Node*> children;
			glm::mat4          matrix;
			std::string        name;
			Mesh*              mesh;
			glm::vec3          translation = {};
			glm::quat          rotation = {};
			glm::vec3          scale = {};

			auto localMatrix() -> glm::mat4
			{
				return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
			}

			auto getTransformationMatrix() -> glm::mat4
			{
				auto local_matrix = localMatrix();
				Node* parent_pointer = parent;
				while(parent_pointer) {
					local_matrix = parent_pointer->localMatrix() * local_matrix;
					parent_pointer = parent_pointer->parent;
				}
				return local_matrix;
			}

			auto update() -> void
			{
				if (mesh) {
					auto transform_matrix = getTransformationMatrix();
					memcpy(mesh->uniformBuffer.mapped, &transform_matrix, sizeof(glm::mat4));
				}

				for (auto& child : children) {
					child->update();
				}
			}

			~Node()
			{
				if (mesh) {
					delete mesh;
				}
				for (auto& child : children) {
					delete child;
				}
			}
		};
	
		struct Model {
			vkpbr::VulkanDevice* device;

			using Vertex = struct
			{
				glm::vec3 position;
				glm::vec3 normal;
				glm::vec2 uv;
			};

			using Vertices = struct
			{
				vk::Buffer buffer;
				vk::DeviceMemory memory;
			};
			Vertices vertices;

			using Indices = struct
			{
				uint32_t count;
				vk::Buffer buffer;
				vk::DeviceMemory memory;
			};
			Indices indices;

			std::vector<Node*> nodes;
			std::vector<Node*> linearNodes;
			std::vector<TextureGLTF> textures;
			std::vector<Material> materials;

			using Dimensions = struct
			{
				glm::vec3 min = glm::vec3(FLT_MAX);
				glm::vec3 max = glm::vec3(-FLT_MAX);
				glm::vec3 size;
				glm::vec3 center;
				float radius;
			};
			Dimensions dimensions;

			auto release(vk::Device device) -> void
			{
				device.destroyBuffer(vertices.buffer, nullptr);
				device.freeMemory(vertices.memory, nullptr);
				device.destroyBuffer(indices.buffer, nullptr);
				device.freeMemory(indices.memory, nullptr);

				for (auto& texture : textures) {
					texture.release();
				}
				for (auto& node : nodes)
				{
					delete node;
				}
			}

			auto loadNode(
				vkpbr::gltf::Node* parent,
				const tinygltf::Node& node,
				uint32_t node_index,
				const tinygltf::Model& model,
				std::vector<uint32_t>& index_buffer,
				std::vector<Vertex>& vertex_buffer,
				float global_scale
			) -> void
			{
				auto* new_node = new Node{};
				new_node->index = node_index;
				new_node->parent = parent;
				new_node->name = node.name;
				new_node->matrix = glm::mat4(1.0f);

				/* Generate local node matrix */
				auto translation = glm::vec3(0.0f);
				if (node.translation.size() == 3) {
					translation = glm::make_vec3(node.translation.data());
					new_node->translation = translation;
				}

				auto rotation = glm::mat4(1.0f);
				if (node.rotation.size() == 4) {
					glm::qua quaternion = glm::make_quat(node.rotation.data());
					new_node->rotation = quaternion;
				}

				auto scale = glm::vec3(1.0f);
				if (node.scale.size() == 3) {
					scale = glm::make_vec3(node.scale.data());
					new_node->scale = scale;
				}

				if (node.matrix.size() == 16) {
					new_node->matrix = glm::make_mat4x4(node.matrix.data());
				}

				/* Node with children */
				if (!node.children.empty()) {
					for (auto i = 0; i < node.children.size(); i++) {
						loadNode(
							new_node,
							model.nodes[node.children[i]],
							node.children[i],
							model,
							index_buffer,
							vertex_buffer,
							global_scale
						);
					}
				}

				if (node.mesh > -1) {
					const auto mesh = model.meshes[node.mesh];
					auto* new_mesh = new Mesh(device, new_node->matrix);

					for (size_t i = 0; i < mesh.primitives.size(); i++) {
						const auto& primitive = mesh.primitives[i];
						if (primitive.indices < 0) {
							continue;
						}

						const auto index_start = static_cast<uint32_t>(index_buffer.size());
						const auto vertex_start = static_cast<uint32_t>(vertex_buffer.size());
						auto index_count = uint32_t{ 0 };
						auto pos_min = glm::vec3{};
						auto pos_max = glm::vec3{};

						/* Vertices */
						{
							const float* buffer_position = nullptr;
							const float* buffer_normals = nullptr;
							const float* buffer_tex_coords = nullptr;

							/* Position is mandatory */
							assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

							const auto& position_accessor = model.accessors[primitive.attributes.find("POSITION")->second];
							const auto& position_view = model.bufferViews[position_accessor.bufferView];
							buffer_position = reinterpret_cast<const float*>(&(model.buffers[position_view.buffer].data[position_accessor.byteOffset + position_view.byteOffset]));
							pos_min = glm::vec3(position_accessor.minValues[0], position_accessor.minValues[1], position_accessor.minValues[2]);
							pos_max = glm::vec3(position_accessor.maxValues[0], position_accessor.maxValues[1], position_accessor.maxValues[2]);

							if (primitive.attributes.find("NORMAl") != primitive.attributes.end()) {
								const auto& normal_accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
								const auto& normal_view = model.bufferViews[normal_accessor.bufferView];
								buffer_normals = reinterpret_cast<const float*>(&(model.buffers[normal_view.buffer].data[normal_accessor.byteOffset + normal_view.byteOffset]));
							}

							if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
								const auto& tex_coord_accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
								const auto& tex_coord_view = model.bufferViews[tex_coord_accessor.bufferView];
								buffer_tex_coords = reinterpret_cast<const float*>(&(model.buffers[tex_coord_view.buffer].data[tex_coord_accessor.byteOffset + tex_coord_view.byteOffset]));
							}

							for (size_t j = 0; j < position_accessor.count; j++) {
								Vertex vertex = {};
								vertex.position = glm::vec3(glm::make_vec3(&buffer_position[j * 3]));
								vertex.normal = glm::normalize(glm::vec3(buffer_normals ? glm::make_vec3(&buffer_normals[j * 3]) : glm::vec3(0.0f)));
								vertex.uv = buffer_tex_coords ? glm::make_vec2(&buffer_tex_coords[j * 2]) : glm::vec2(0.0f);
								vertex_buffer.push_back(vertex);
							}
						}

						/* Indices */
						{
							auto& accessor = model.accessors[primitive.indices];
							auto& buffer_view = model.bufferViews[accessor.bufferView];
							auto& buffer = model.buffers[buffer_view.buffer];

							index_count = static_cast<uint32_t>(accessor.count);

							switch (accessor.componentType) {
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
								auto* data_buffer = new uint32_t[accessor.count];
								memcpy(data_buffer, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint32_t));
								for (size_t index = 0; index < accessor.count; index++) {
									index_buffer.push_back(data_buffer[index] + vertex_start);
								}
								break;
							}
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
								auto* data_buffer = new uint16_t[accessor.count];
								memcpy(data_buffer, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count + sizeof(uint16_t));
								for(size_t index = 0; index < accessor.count; index++) {
									index_buffer.push_back(data_buffer[index] + vertex_start);
								}
								break;
							}
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
								auto* data_buffer = new uint8_t[accessor.count];
								memcpy(data_buffer, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count + sizeof(uint8_t));
								for (size_t index = 0; index < accessor.count; index++) {
									index_buffer.push_back(data_buffer[index] + vertex_start);
								}
								break;
							}
							default:
								std::cerr << "Model index type " << accessor.componentType << " not supported!" << std::endl;
								return;
							}
						}
						auto* new_primitive = new Primitive(index_start, index_count, materials[primitive.material]);
						new_primitive->setDimensions(pos_min, pos_max);
						new_mesh->primitives.push_back(new_primitive);
					}
					new_node->mesh = new_mesh;
				}

				if (parent) {
					parent->children.push_back(new_node);
				}
				else {
					nodes.push_back(new_node);
				}
				linearNodes.push_back(new_node);
			}

			auto loadImages(tinygltf::Model& model, vkpbr::VulkanDevice* device, vk::Queue transfer_queue) -> void
			{
				for (auto& image : model.images) {
					vkpbr::TextureGLTF texture;
					texture.loadFromGLTFImage(image, device, transfer_queue);
					textures.push_back(texture);
				}
			}

			auto loadMaterials(tinygltf::Model& model) -> void
			{
				for (tinygltf::Material& material : model.materials) {
					auto new_material = vkpbr::gltf::Material{};

					if (material.values.find("baseColorTexture") != material.values.end()) {
						new_material.baseColorTexture = &textures[model.textures[material.values["baseColorTexture"].TextureIndex()].source];
					}

					if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
						new_material.metallicRoughnessTexture = &textures[model.textures[material.values["metallicRoughnessTexture"].TextureIndex()].source];
					}

					if (material.values.find("roughnessFactor") != material.values.end()) {
						new_material.roughnessFactor = static_cast<float>(material.values["roughnessFactor"].Factor());
					}

					if (material.values.find("metallicFactor") != material.values.end()) {
						new_material.metallicFactor = static_cast<float>(material.values["metallicFactor"].Factor());
					}

					if (material.values.find("baseColorFactor") != material.values.end()) {
						new_material.baseColorFactor = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());
					}

					if (material.additionalValues.find("normalTexture") != material.additionalValues.end()) {
						new_material.normalTexture = &textures[model.textures[material.additionalValues["normalTexture"].TextureIndex()].source];
					}

					if (material.additionalValues.find("emissiveTexture") != material.additionalValues.end()) {
						new_material.emissiveTexture = &textures[model.textures[material.additionalValues["emissiveTexture"].TextureIndex()].source];
					}

					if (material.additionalValues.find("occlusionTexture") != material.additionalValues.end()) {
						new_material.occlusionTexture = &textures[model.textures[material.additionalValues["occlusionTexture"].TextureIndex()].source];
					}

					if (material.additionalValues.find("alphaMode") != material.additionalValues.end()) {
						auto parameters = material.additionalValues["alphaMode"];
						if (parameters.string_value == "BLEND") {
							new_material.alphaMode = Material::AlphaMode::blend;
						}
						if (parameters.string_value == "MASK") {
							new_material.alphaMode = Material::AlphaMode::mask;
						}
					}
					
					if (material.additionalValues.find("alphaCutoff") != material.additionalValues.end()) {
						new_material.alphaCutoff = static_cast<float>(material.additionalValues["alphaCutoff"].Factor());
					}

					if (material.additionalValues.find("emissiveFactor") != material.additionalValues.end()) {
						new_material.emissiveFactor = glm::vec4(glm::make_vec3(material.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0f);
						new_material.emissiveFactor = glm::vec4(0.0f);
					}

					// TODO: dodelat extensions
					materials.push_back(new_material);
				}
			}

			auto loadFromFile(const std::string& filename, vkpbr::VulkanDevice* device, vk::Queue transfer_queue, float scale = 1.0f) -> void
			{
				auto gltf_model = tinygltf::Model{};
				auto gltf_context = tinygltf::TinyGLTF{};
				auto error_string = std::string{};
				auto warning_string = std::string{};

				this->device = device;
				auto index_buffer = std::vector<uint32_t>{};
				auto vertex_buffer = std::vector<Vertex>{};

				const auto file_loaded = gltf_context.LoadASCIIFromFile(&gltf_model, &error_string, &warning_string, filename.c_str());

				if (file_loaded) {
					loadImages(gltf_model, device, transfer_queue);
					loadMaterials(gltf_model);
					const auto& scene = gltf_model.scenes[gltf_model.defaultScene];

					for (size_t i = 0; i < scene.nodes.size(); i++) {
						const auto node = gltf_model.nodes[scene.nodes[i]];
						loadNode(nullptr, node, scene.nodes[i], gltf_model, index_buffer, vertex_buffer, scale);
					}
				}
				else {
					std::cerr << "Could not load GLTF file!" << std::endl;
					exit(EXIT_FAILURE); //TODO: predelat na throw
				}

				const auto vertex_buffer_size = vertex_buffer.size() * sizeof(Vertex);
				const auto index_buffer_size = index_buffer.size() * sizeof(uint32_t);
				indices.count = static_cast<uint32_t>(index_buffer.size());

				assert((vertex_buffer_size > 0) && (index_buffer_size > 0));

				using StagingBuffer = struct {
					vk::Buffer       buffer;
					vk::DeviceMemory memory;
				};

				auto vertex_staging_buffer = StagingBuffer{};
				auto index_staging_buffer = StagingBuffer{};

				VK_ASSERT(device->createBuffer(
					vertex_buffer_size,
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					vertex_staging_buffer.buffer,
					vertex_staging_buffer.memory,
					vertex_buffer.data()
				));

				VK_ASSERT(device->createBuffer(
					index_buffer_size,
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					index_staging_buffer.buffer,
					index_staging_buffer.memory,
					index_buffer.data()
				));

				/* Device local buffers */

				VK_ASSERT(device->createBuffer(
					vertex_buffer_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
					vk::MemoryPropertyFlagBits::eDeviceLocal,
					vertices.buffer,
					vertices.memory
				));

				VK_ASSERT(device->createBuffer(
					index_buffer_size,
					vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
					vk::MemoryPropertyFlagBits::eDeviceLocal,
					indices.buffer,
					indices.memory
				));
				
				auto copy_cmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);
				auto copy_region = vk::BufferCopy{};

				copy_region.size = vertex_buffer_size;
				copy_cmd.copyBuffer(vertex_staging_buffer.buffer, vertices.buffer, 1, &copy_region);

				copy_region.size = index_buffer_size;
				copy_cmd.copyBuffer(index_staging_buffer.buffer, indices.buffer, 1, &copy_region);

				device->finishAndSubmitCmdBuffer(copy_cmd, transfer_queue, true);

				device->logicalDevice.destroyBuffer(vertex_staging_buffer.buffer, nullptr);
				device->logicalDevice.freeMemory(vertex_staging_buffer.memory, nullptr);
				device->logicalDevice.destroyBuffer(index_staging_buffer.buffer, nullptr);
				device->logicalDevice.freeMemory(index_staging_buffer.memory, nullptr);

				setSceneDimensions();
			}

			auto drawNode(Node* node, vk::CommandBuffer cmd_buffer) const -> void
			{
				if (node->mesh) {
					for (Primitive* primitive : node->mesh->primitives) {
						cmd_buffer.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
					}
				}

				for (auto& child : node->children) {
					drawNode(child, cmd_buffer);
				}
			}

			auto draw(vk::CommandBuffer cmd_buffer) -> void
			{
				const vk::DeviceSize offsets[1] = { 0 };
				cmd_buffer.bindVertexBuffers(0, 1, &vertices.buffer, offsets);
				cmd_buffer.bindIndexBuffer(indices.buffer, 0, vk::IndexType::eUint32);

				for (auto& node : nodes) {
					drawNode(node, cmd_buffer);
				}
			}

			auto getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max) const -> void
			{
				if (node->mesh) {
					for (auto* primitive : node->mesh->primitives) {
						auto loc_min = glm::vec4(primitive->dimensions.min, 1.0f) * node->getTransformationMatrix();
						auto loc_max = glm::vec4(primitive->dimensions.max, 1.0f) * node->getTransformationMatrix();

						if (loc_min.x < min.x) { min.x = loc_min.x; }
						if (loc_min.y < min.y) { min.y = loc_min.y; }
						if (loc_min.z < min.z) { min.z = loc_min.z; }

						if (loc_max.x > max.x) { max.x = loc_max.x; }
						if (loc_max.y > max.y) { max.y = loc_max.y; }
						if (loc_max.z > max.z) { max.z = loc_max.z; }
					}
				}

				for (auto& child : node->children) {
					getNodeDimensions(child, min, max);
				}
			}

			auto setSceneDimensions() -> void
			{
				dimensions.min = glm::vec3(FLT_MAX);
				dimensions.max = glm::vec3(-FLT_MAX);

				for (auto& node : nodes) {
					getNodeDimensions(node, dimensions.min, dimensions.max);
				}

				dimensions.size = dimensions.max - dimensions.min;
				dimensions.center = (dimensions.min + dimensions.max) / 2.0f;
				dimensions.radius = glm::distance(dimensions.min, dimensions.max) / 2.0f;
			}
			
			auto findNode(Node* parent, const uint32_t index) const -> Node*
			{
				Node* found_node = nullptr;
				if (parent->index == index) {
					return parent;
				}

				for (auto& child : parent->children) {
					found_node = findNode(child, index);
					if (found_node) {
						break;
					}
				}
				return found_node;
			}

			auto nodeFromIndex(const uint32_t index) -> Node*
			{
				Node* found_node = nullptr;

				for (auto& node : nodes) {
					found_node = findNode(node, index);
					if (found_node) {
						break;
					}
				}

				return found_node;
			}
		};
	}
}