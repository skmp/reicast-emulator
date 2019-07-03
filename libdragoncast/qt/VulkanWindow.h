/*
*	qt/VulkanWindow.h
*/
#pragma once


#include <QVulkanWindow>
#include <QLoggingCategory>
#include "VulkanRenderer.h"



class VulkanWindow
	: public QVulkanWindow
{
	Q_OBJECT

	VulkanRenderer* m_renderer;

	bool m_debug;

public:
	VulkanWindow(bool dbg)
		: m_debug(dbg)
		, m_renderer(nullptr)
	{

	}


	// This is called by Renderer::Create() indirectly ! //
	QVulkanWindowRenderer* createRenderer() override
	{
	//	return nullptr;
		qDebug() << __FUNCTION__"()";

		if (!m_renderer)
			m_renderer = new VulkanRenderer(this);

		return m_renderer;
	}

	bool isDebug() const { return m_debug; }



	static QVulkanInstance * CreateVkInstance(bool dbg)
	{
		//qEnvironmentVariableIntValue("QT_VK_DEBUG");

		QVulkanInstance *inst = new QVulkanInstance;

		if (dbg) {
			QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

#ifndef Q_OS_ANDROID
			inst->setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
#else
			inst->setLayers(QByteArrayList()
				<< "VK_LAYER_GOOGLE_threading"
				<< "VK_LAYER_LUNARG_parameter_validation"
				<< "VK_LAYER_LUNARG_object_tracker"
				<< "VK_LAYER_LUNARG_core_validation"
				<< "VK_LAYER_LUNARG_image"
				<< "VK_LAYER_LUNARG_swapchain"
				<< "VK_LAYER_GOOGLE_unique_objects");
#endif
		}

		if (!inst->create()) {
			qFatal("Failed to create Vulkan instance: %d", inst->errorCode());
			return nullptr;
		}

		return inst;
	}





};





