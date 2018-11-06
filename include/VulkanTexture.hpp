#pragma once

#include <vulkan/vulkan.hpp>

#include <gli/gli.hpp>

#include <VulkanDevice.hpp>
#include <Utility.hpp>

/*
VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Optimal for presentation
VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Optimal as attachment for writing colors from the fragment shader
VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: Optimal as source in a transfer operation, like vkCmdCopyImageToBuffer
VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Optimal as destination in a transfer operation, like vkCmdCopyBufferToImage
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: Optimal for sampling from a shader
*/

namespace vkpbr {

	class Texture {
	public:
		vkpbr::VulkanDevice*    device;
		vk::Image               image;
		vk::ImageView           imageView;
		vk::ImageLayout         imageLayout;
		vk::DeviceMemory        deviceMemory;
		vk::DescriptorImageInfo descriptorInfo;
		vk::Sampler             textureSampler;
		uint32_t                width;
		uint32_t                height;
		uint32_t                mipLevels;
		uint32_t                layerr;

		auto updateDescriptorInfo() -> void
		{
			descriptorInfo.sampler = textureSampler;
			descriptorInfo.imageView = imageView;
			descriptorInfo.imageLayout = imageLayout;
		}

		auto release() const -> void
		{
			std::cout << "I am destroying brother" << std::endl;
			device->logicalDevice.destroyImageView(imageView, nullptr);
			device->logicalDevice.destroyImage(image, nullptr);
			if (textureSampler) {
				device->logicalDevice.destroySampler(textureSampler, nullptr);
			}
			device->logicalDevice.freeMemory(deviceMemory, nullptr);
		}
	};


	class Texture2D : public Texture {
	public:

		auto loadFromFile(
			const std::string& filename,
			vk::Format format,
			vkpbr::VulkanDevice* device,
			vk::Queue loading_queue,
			const vk::ImageUsageFlags image_usage = vk::ImageUsageFlagBits::eSampled,
			const vk::ImageLayout image_layout = vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			gli::texture2d texture(gli::load(filename.c_str()));
			assert(!texture.empty());

			this->device = device;
			width = static_cast<uint32_t>(texture[0].extent().x);
			height = static_cast<uint32_t>(texture[0].extent().y);
			mipLevels = static_cast<uint32_t>(texture.levels());

			/* Prepare texture loading command, buffer and memory */
			auto[loading_cmd, staging_buffer, staging_memory] =  textureLoadingCommand(format, texture);
			auto buffer_regions = setupBufferCopyRegions(texture);

			createImage(format, image_usage);
			bindImageMemory();

			vk::ImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = mipLevels;
			subresource_range.layerCount = 1;

			/* Memory barrier */
			{
				vk::ImageMemoryBarrier memory_barrier = {};
				memory_barrier.oldLayout = vk::ImageLayout::eUndefined;
				memory_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				memory_barrier.srcAccessMask = static_cast<vk::AccessFlagBits>(0);
				memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				memory_barrier.image = image;
				memory_barrier.subresourceRange = subresource_range;
				loading_cmd.pipelineBarrier(
					vk::PipelineStageFlagBits::eAllCommands,
					vk::PipelineStageFlagBits::eAllCommands,
					static_cast<vk::DependencyFlagBits>(0),
					0,
					nullptr,
					0,
					nullptr,
					1,
					&memory_barrier
				);
			}

			/* Copy mip levels from buffer */
			loading_cmd.copyBufferToImage(
				staging_buffer,
				image,
				vk::ImageLayout::eTransferDstOptimal,
				static_cast<uint32_t>(buffer_regions.size()),
				buffer_regions.data()
			);

			this->imageLayout = image_layout;
			/* Memory barrier */
			{
				vk::ImageMemoryBarrier memory_barrier = {};
				memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				memory_barrier.newLayout = imageLayout;
				memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
				memory_barrier.image = image;
				memory_barrier.subresourceRange = subresource_range;
				loading_cmd.pipelineBarrier(
					vk::PipelineStageFlagBits::eAllCommands,
					vk::PipelineStageFlagBits::eAllCommands,
					static_cast<vk::DependencyFlagBits>(0),
					0,
					nullptr,
					0,
					nullptr,
					1,
					&memory_barrier
				);
			}

			device->finishAndSubmitCmdBuffer(loading_cmd, loading_queue);

			device->logicalDevice.freeMemory(staging_memory, nullptr);
			device->logicalDevice.destroyBuffer(staging_buffer, nullptr);

			createTextureSampler();
			createImageView(format);
			
			updateDescriptorInfo();
		}

		auto textureLoadingCommand(vk::Format& format, const gli::texture2d& texture) const -> std::tuple<vk::CommandBuffer, vk::Buffer, vk::DeviceMemory>
		{
			/* Get device properties */
			vk::FormatProperties format_properties;
			device->physicalDevice.getFormatProperties(format, &format_properties);

			vk::MemoryAllocateInfo allocate_info = {};
			vk::MemoryRequirements memory_requirements;

			vk::CommandBuffer loadingCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

			vk::Buffer staging_buffer;
			vk::DeviceMemory staging_memory;

			vk::BufferCreateInfo buffer_create_info = {};
			buffer_create_info.size = texture.size();
			buffer_create_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
			buffer_create_info.sharingMode = vk::SharingMode::eExclusive;

			VK_CHECK_RESULT(device->logicalDevice.createBuffer(&buffer_create_info, nullptr, &staging_buffer));
			device->logicalDevice.getBufferMemoryRequirements(staging_buffer, &memory_requirements);

			allocate_info.allocationSize = memory_requirements.size;
			allocate_info.memoryTypeIndex = device->findMemoryType(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			VK_CHECK_RESULT(device->logicalDevice.allocateMemory(&allocate_info, nullptr, &staging_memory));
			device->logicalDevice.bindBufferMemory(staging_buffer, staging_memory, 0);

			// Copy texture data
			uint8_t* data;
			VK_CHECK_RESULT(device->logicalDevice.mapMemory(staging_memory, 0, memory_requirements.size, vk::MemoryMapFlagBits(), reinterpret_cast<void**>(&data)));
			memcpy(data, texture.data(), texture.size());
			device->logicalDevice.unmapMemory(staging_memory);

			return std::make_tuple(loadingCmd, staging_buffer, staging_memory);
		}
	
		auto createImage(const vk::Format& format, const vk::ImageUsageFlags& image_usage) -> void
		{
			vk::ImageCreateInfo image_create_info = {};
			image_create_info.imageType = vk::ImageType::e2D;
			image_create_info.format = format;
			image_create_info.mipLevels = mipLevels;
			image_create_info.arrayLayers = 1;
			image_create_info.samples = vk::SampleCountFlagBits::e1;
			image_create_info.tiling = vk::ImageTiling::eOptimal;
			image_create_info.sharingMode = vk::SharingMode::eExclusive;
			image_create_info.initialLayout = vk::ImageLayout::eUndefined;
			image_create_info.extent = vk::Extent3D(width, height, 1);
			image_create_info.usage = image_usage;
			if (!(image_create_info.usage & vk::ImageUsageFlagBits::eTransferDst)) {
				image_create_info.usage |= vk::ImageUsageFlagBits::eTransferDst;
			}
			VK_CHECK_RESULT(device->logicalDevice.createImage(&image_create_info, nullptr, &image));
			
		}

		auto setupBufferCopyRegions(const gli::texture2d& texture) const -> std::vector<vk::BufferImageCopy>
		{
			auto buffer_regions = std::vector<vk::BufferImageCopy>();
			uint32_t offset = 0;
			for (uint32_t i = 0; i < mipLevels; i++) {
				vk::BufferImageCopy buffer_image_copy = {};
				buffer_image_copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				buffer_image_copy.imageSubresource.mipLevel = i;
				buffer_image_copy.imageSubresource.baseArrayLayer = 0;
				buffer_image_copy.imageSubresource.layerCount = 1;
				buffer_image_copy.imageExtent.width = static_cast<uint32_t>(texture[i].extent().x);
				buffer_image_copy.imageExtent.height = static_cast<uint32_t>(texture[i].extent().y);
				buffer_image_copy.imageExtent.depth = 1;
				buffer_image_copy.bufferOffset = offset;

				buffer_regions.push_back(buffer_image_copy);
				offset += static_cast<uint32_t>(texture[i].size());
			}
			return buffer_regions;
		}

		auto bindImageMemory() -> void
		{
			vk::MemoryRequirements memory_requirements;
			vk::MemoryAllocateInfo allocate_info = {};

			device->logicalDevice.getImageMemoryRequirements(image, &memory_requirements);
			allocate_info.allocationSize = memory_requirements.size;
			allocate_info.memoryTypeIndex = device->findMemoryType(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
			
			VK_CHECK_RESULT(device->logicalDevice.allocateMemory(&allocate_info, nullptr, &deviceMemory));
			device->logicalDevice.bindImageMemory(image, deviceMemory, 0);
		}

		auto createTextureSampler() -> void
		{
			vk::SamplerCreateInfo sampler_create_info = {};
			sampler_create_info.magFilter = vk::Filter::eLinear;
			sampler_create_info.minFilter = vk::Filter::eLinear;
			sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
			sampler_create_info.addressModeU = vk::SamplerAddressMode::eRepeat;
			sampler_create_info.addressModeV = vk::SamplerAddressMode::eRepeat;
			sampler_create_info.addressModeW = vk::SamplerAddressMode::eRepeat;
			sampler_create_info.mipLodBias = 0.0f;
			sampler_create_info.compareOp = vk::CompareOp::eNever;
			sampler_create_info.minLod = 0.0f;
			sampler_create_info.maxLod = static_cast<float>(mipLevels);
			sampler_create_info.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
			sampler_create_info.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->deviceProperties.limits.maxSamplerAnisotropy : 1.0f;
			sampler_create_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
			VK_CHECK_RESULT(device->logicalDevice.createSampler(&sampler_create_info, nullptr, &textureSampler));
		}
	
		auto createImageView(vk::Format format) -> void
		{
			vk::ImageViewCreateInfo view_create_info = {};
			view_create_info.viewType = vk::ImageViewType::e2D;
			view_create_info.format = format;
			view_create_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,  vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
			view_create_info.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
			view_create_info.subresourceRange.levelCount = mipLevels;
			view_create_info.image = image;
			VK_CHECK_RESULT(device->logicalDevice.createImageView(&view_create_info, nullptr, &imageView));
		}
	};
}