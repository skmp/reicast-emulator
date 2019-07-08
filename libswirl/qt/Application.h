/*
*	qt/Application.h
*/
#pragma once

#include <QApplication>
#include <QOpenGLWindow>

#include "MainWindow.h"

#include "Renderer_if.h"


class Application
	: public QApplication
{
	Q_OBJECT

	bool m_dbg = true;

	MainWindow  m_mainWindow;

public:

	eRenderer m_renderer = R_Vulkan;	// R_GL4; // R_Vulkan;


	~Application()
	{
#if 0
		if (m_mainWindow)
			m_mainWindow->deleteLater();
#endif
	}

	Application(int& argc, char** argv)
		: QApplication(argc,argv)
	{	}


	static Application* Get() {
		return qobject_cast<Application*>(qApp);
	}

	bool isDebug() { return m_dbg; }

	MainWindow* mainWindow() {
		return &m_mainWindow;
	}


	/*
	*	createMainWindow(): this will prob eventually be replaced, lettings the platforms handle full init just giving native handles ;sigh;
	*/
	bool createViewWindow()
	{
		QWindow* viewWindow = nullptr;

		if (R_Vulkan == m_renderer) {
			VulkanWindow* vulkanWindow = new VulkanWindow(m_dbg);
			if (vulkanWindow) {
				vulkanWindow->setVulkanInstance(VulkanWindow::CreateVkInstance(m_dbg));
				viewWindow = (QWindow*)vulkanWindow;
			}
			else return false;
		}
		else {
			OpenGLWindow* glWindow = new OpenGLWindow();
			if (!glWindow) return false;
			viewWindow = (QWindow*)glWindow;
			
		}
		if (nullptr != viewWindow) {
			m_mainWindow.setViewWindow(viewWindow);
			return true;
		}
		return false;
	}



	inline QWindow* rendWindow() {
		return m_mainWindow.rendWindow();
	}
	inline OpenGLWindow* glWindow() {
		return m_mainWindow.glWindow();
	}
	inline VulkanWindow* vkWindow() {
		return m_mainWindow.vkWindow();
	}


	inline bool isGL() { return nullptr != glWindow(); }
	inline bool isVK() { return nullptr != vkWindow(); }


};

#define reiApp Application::Get()






