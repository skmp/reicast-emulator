/*
    Created on: Oct 2, 2019

	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "vulkan_context.h"
#include "hw/pvr/Renderer_if.h"
#include "compiler.h"

VulkanContext *VulkanContext::contextInstance;
PFN_vkGetInstanceProcAddr vulkan_symbol_wrapper_vkGetInstanceProcAddr;

static const char *PipelineCacheFileName = "vulkan_pipeline.cache";

bool VulkanContext::Init(retro_hw_render_interface_vulkan *retro_render_if)
{
	if (retro_render_if->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN
			|| retro_render_if->interface_version != RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
		return false;
	this->retro_render_if = retro_render_if;

	instance = retro_render_if->instance;
	physicalDevice = retro_render_if->gpu;
	device = retro_render_if->device;
	queue = retro_render_if->queue;

	vulkan_symbol_wrapper_vkGetInstanceProcAddr = retro_render_if->get_instance_proc_addr;
	vulkan_symbol_wrapper_init(retro_render_if->get_instance_proc_addr);
	vulkan_symbol_wrapper_load_global_symbols();
	vulkan_symbol_wrapper_load_core_symbols(instance);
	vulkan_symbol_wrapper_load_core_device_symbols(device);

	vk::PhysicalDeviceProperties properties;
	physicalDevice.getProperties(&properties);
	uniformBufferAlignment = properties.limits.minUniformBufferOffsetAlignment;
	storageBufferAlignment = properties.limits.minStorageBufferOffsetAlignment;

	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G5B5A1UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported1555 = true;
	else
		NOTICE_LOG(RENDERER, "eR5G5B5A1UnormPack16 not supported for optimal tiling");
	formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G6B5UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported565 = true;
	else
		NOTICE_LOG(RENDERER, "eR5G6B5UnormPack16 not supported for optimal tiling");
	formatProperties = physicalDevice.getFormatProperties(vk::Format::eR4G4B4A4UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported4444 = true;
	else
		NOTICE_LOG(RENDERER, "eR4G4B4A4UnormPack16 not supported for optimal tiling");
	vk::PhysicalDeviceFeatures features;
	physicalDevice.getFeatures(&features);
	fragmentStoresAndAtomics = features.fragmentStoresAndAtomics;
	samplerAnisotropy = features.samplerAnisotropy;
	unifiedMemory = properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;

	ShaderCompiler::Init();

   // Descriptor pool
	vk::DescriptorPoolSize pool_sizes[] =
	{
		{ vk::DescriptorType::eSampler, 2 },
      { vk::DescriptorType::eCombinedImageSampler, 4000 },
      { vk::DescriptorType::eSampledImage, 2 },
      { vk::DescriptorType::eStorageImage, 12 },
      { vk::DescriptorType::eUniformTexelBuffer, 2 },
		{ vk::DescriptorType::eStorageTexelBuffer, 2 },
      { vk::DescriptorType::eUniformBuffer, 36 },
      { vk::DescriptorType::eStorageBuffer, 36 },
      { vk::DescriptorType::eUniformBufferDynamic, 2 },
      { vk::DescriptorType::eStorageBufferDynamic, 2 },
      { vk::DescriptorType::eInputAttachment, 36 }
   };
	descriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
   		10000, ARRAY_SIZE(pool_sizes), pool_sizes));

   std::string cachePath = get_writable_data_path(PipelineCacheFileName);
   FILE *f = fopen(cachePath.c_str(), "rb");
   if (f == nullptr)
   	pipelineCache = device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
   else
   {
   	fseek(f, 0, SEEK_END);
   	size_t cacheSize = ftell(f);
   	fseek(f, 0, SEEK_SET);
   	u8 *cacheData = new u8[cacheSize];
   	if (fread(cacheData, 1, cacheSize, f) != cacheSize)
   		cacheSize = 0;
   	fclose(f);
		pipelineCache = device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo(vk::PipelineCacheCreateFlags(), cacheSize, cacheData));
		delete [] cacheData;
		INFO_LOG(RENDERER, "Vulkan pipeline cache loaded from %s: %zd bytes", cachePath.c_str(), cacheSize);
   }
   allocator.Init(physicalDevice, device);
   FindDepthFormat();

	retro_image.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	retro_image.create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_image.create_info.pNext = nullptr;
	retro_image.create_info.format = (VkFormat)colorFormat;
	retro_image.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_image.create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_image.create_info.subresourceRange.baseArrayLayer = 0;
	retro_image.create_info.subresourceRange.layerCount = 1;
	retro_image.create_info.subresourceRange.baseMipLevel = 0;
	retro_image.create_info.subresourceRange.levelCount = 1;
	retro_image.create_info.flags = 0;

	return true;
}

vk::Format VulkanContext::FindDepthFormat()
{
	const vk::Format depthFormats[] = { vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint };
	vk::ImageTiling tiling;
	depthFormat = vk::Format::eUndefined;
	for (int i = 0; i < ARRAY_SIZE(depthFormats); i++)
	{
		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(depthFormats[i]);

		if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			tiling = vk::ImageTiling::eOptimal;
			depthFormat = depthFormats[i];
			break;
		}
	}
	if (depthFormat == vk::Format::eUndefined)
	{
		// Try to find a linear format
		for (int i = 0; i < ARRAY_SIZE(depthFormats); i++)
		{
			vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(depthFormats[i]);

			if (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
			{
				tiling = vk::ImageTiling::eLinear;
				depthFormat = depthFormats[i];
				break;
			}
		}
		if (depthFormat == vk::Format::eUndefined)
			die("No supported depth/stencil format found");
	}
	NOTICE_LOG(RENDERER, "Using depth format %s tiling %s", vk::to_string(depthFormat).c_str(), vk::to_string(tiling).c_str());

	return depthFormat;
}

void VulkanContext::PresentFrame(vk::Image image, vk::ImageView imageView, vk::Offset2D extent)
{
	retro_image.image_view = (VkImageView)imageView;
	retro_image.create_info.image = (VkImage)image;
	retro_render_if->set_image(retro_render_if->handle, &retro_image, 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
}

void VulkanContext::Term()
{
	if (device && pipelineCache)
   {
		std::vector<u8> cacheData = device.getPipelineCacheData(*pipelineCache);
		if (!cacheData.empty())
		{
			std::string cachePath = get_writable_data_path(PipelineCacheFileName);
			FILE *f = fopen(cachePath.c_str(), "wb");
			if (f != nullptr)
			{
				(void)fwrite(&cacheData[0], 1, cacheData.size(), f);
				fclose(f);
			}
		}
   }
	ShaderCompiler::Term();
	descriptorPool.reset();
	allocator.Term();
	pipelineCache.reset();
}
