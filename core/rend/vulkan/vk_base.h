/*
*	vk_base.h
*/
#pragma once

#include "vulkan/vulkan.hpp"	// VK_USE_PLATFORM_* defined in CMake


#include <QVulkanFunctions>

#include "qt/Application.h"
#include "qt/VulkanWindow.h"








class VulkanBuffer
{
	VkDevice m_dev;
	VkBuffer m_buffer;
	VkDeviceMemory m_devMemory;

	size_t m_size;// , m_align;

	void* m_map;

public:

	~VulkanBuffer() {
		auto window = reiApp->vkWindow();
		if (window) {
			VkDevice dev = window->device();
			QVulkanInstance* inst = window->vulkanInstance();
			auto vkFuncs = inst->deviceFunctions(dev);
			vkFuncs->vkDestroyBuffer(dev, m_buffer, nullptr);
			vkFuncs->vkFreeMemory(dev, m_devMemory, nullptr);
		}
	}

	VulkanBuffer(size_t size, VkBufferUsageFlags usage)//, size_t align = 0)
		: m_devMemory(nullptr)
		, m_buffer(nullptr)
		, m_size(size)
		, m_map(nullptr)
		, m_dev(nullptr)
	//	, m_align(align)
	{
		auto window = reiApp->vkWindow();
		if (!window) {
			qFatal(__FUNCTION__"(): failed to get VulkanWindow!");
			return;
		}

		QVulkanInstance* inst = window->vulkanInstance();


		m_dev = window->device();
		auto vkFuncs = inst->deviceFunctions(m_dev);


		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size			= m_size;
		bufferInfo.usage		= usage;
		bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		if (VK_SUCCESS != vkFuncs->vkCreateBuffer(m_dev, &bufferInfo, nullptr, &m_buffer)) {
			throw std::runtime_error("failed to create vertex buffer!");
		}

		VkMemoryRequirements memReqs;
		vkFuncs->vkGetBufferMemoryRequirements(m_dev, m_buffer, &memReqs);

		
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
#if 1
		allocInfo.memoryTypeIndex = window->hostVisibleMemoryIndex();
#else
		VkPhysicalDevice phyDev = window->physicalDevice();
		VkPhysicalDeviceMemoryProperties memProperties;
		vkFuncs->vkGetPhysicalDeviceMemoryProperties(phyDev, &memProperties);

		unat props = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((memReqs.memoryTypeBits & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & props) == props)
			{
				allocInfo.memoryTypeIndex = i;
				break;
			}
		}
#endif

		if (VK_SUCCESS != vkFuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, &m_devMemory)) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}
		//If memory allocation was successful, then we can now associate this memory with the buffer using vkBindBufferMemory:

		vkFuncs->vkBindBufferMemory(m_dev, m_buffer, m_devMemory, 0);
	}

	void* map()
	{
		if (m_map)
			return m_map;

		auto window = reiApp->vkWindow();
		QVulkanInstance* inst = window->vulkanInstance();
		auto vkFuncs = inst->deviceFunctions(m_dev);

		if (VK_SUCCESS != vkFuncs->vkMapMemory(m_dev, m_devMemory, 0, m_size, 0, &m_map))
			return nullptr;

		printf("%(): got address: %p \n", __FUNCTION__, m_map);

		return m_map;
	}


	VkBuffer* get() {
		return &m_buffer;
	}
	VkBuffer _get() {
		return m_buffer;
	}

};








