/*
*	qt/VulkanRenderer.h
*/
#pragma once


#include <QVulkanWindowRenderer>



class VulkanWindow;

class VulkanRenderer
	: public QVulkanWindowRenderer
{
	VulkanWindow* m_window;



public:
	VulkanRenderer(VulkanWindow* w); //, int initialCount);

	void initResources() override;
	void releaseResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;

	void startNextFrame() override;


};
