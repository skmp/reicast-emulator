/*
*	qt/MainWindow.h
*/
#pragma once

#include <QtGui>
#include <QtWidgets>
#include <QKeyEvent>

#include "VulkanWindow.h"
#include "OpenGLWindow.h"

#include "GfxDebugger.h"



class MainWindow
	: public QMainWindow
{
	Q_OBJECT

	QWidget* container = nullptr;
	QWindow* view = nullptr;
	

	GfxDebugger *m_gfxDebugger;
//	Sh4Debugger *m_sh4Debugger;
//	ArmDebugger *m_armDebugger;
//	MemViewer   *m_memViewer;
public:

	MainWindow(QWidget *parent = nullptr)
		: QMainWindow(parent)
	{
		setAttribute(Qt::WA_NativeWindow);
		setMinimumSize(QSize(640, 480));

		setupMenu();
		show();
	}

	MainWindow(QWindow* viewWin)
		: QMainWindow()
		, view(viewWin)
	{
		setAttribute(Qt::WA_NativeWindow);
		setMinimumSize(QSize(640, 480));
		setViewWindow(viewWin);
		show();
	}

	~MainWindow()
	{

		if (m_gfxDebugger)
			m_gfxDebugger->deleteLater();



	}

	void setViewWindow(QWindow* viewWindow)
	{
		view = viewWindow;

		if (container)
			container->deleteLater();

		container = QWidget::createWindowContainer(view, this);
		QSize screenSize = view->screen()->size();
		container->resize(screenSize);
		setCentralWidget(container);
	}

	inline QWindow* rendWindow() {
		return view;
	}

	inline OpenGLWindow* glWindow() {
		return qobject_cast<OpenGLWindow*>(view);
	}

	inline VulkanWindow* vkWindow() {
		return qobject_cast<VulkanWindow*>(view);
	}

	inline bool isGL() { return nullptr != glWindow(); }
	inline bool isVK() { return nullptr != vkWindow(); }


protected:


//	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event)
	{
		if (Qt::Key_Escape == event->key())
			qApp->quit();
	}



private:

	void setupMenu()
	{
		QMenuBar * menu = menuBar();



		QAction *aExit = new QAction(tr("E&xit"), this);
		aExit->setShortcuts(QKeySequence::Quit);
		aExit->setStatusTip(tr("Exit the emulator"));

		connect(aExit, &QAction::triggered, this, &MainWindow::close);


		QMenu *fileMenu = menu->addMenu(tr("&File"));
		fileMenu->addAction(aExit);


		QMenu* emuMenu = menu->addMenu(tr("&Emulator"));
		emuMenu->addAction("wip");

		QMenu *viewMenu = menu->addMenu(tr("&View"));
		viewMenu->addAction("Show &GfxDebugger");

		QMenu* helpMenu = menu->addMenu(tr("&Help"));
		helpMenu->addAction("&about");





	}

};












