/*
*	qt/OpenGLWindow.cpp
*/
#include <QtCore>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QPainter>

#include "Application.h"

#if 1

 
OpenGLWindow* OpenGLWindow::Get()
{
	if (reiApp && reiApp->mainWindow())
		return reiApp->mainWindow()->glWindow();

	return nullptr;
}


OpenGLWindow::OpenGLWindow(QWindow* parent)
	: QWindow(parent)
	, nativeCtx()
{
	setSurfaceType(QWindow::OpenGLSurface);

}

OpenGLWindow::~OpenGLWindow()
{
	delete m_device;
}


void OpenGLWindow::render(QPainter* painter)
{
	Q_UNUSED(painter);
}

void OpenGLWindow::initialize()
{

}



void OpenGLWindow::render()
{
	if (!m_device)
		m_device = new QOpenGLPaintDevice;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	m_device->setSize(size() * devicePixelRatio());
	m_device->setDevicePixelRatio(devicePixelRatio());

	QPainter painter(m_device);
	render(&painter);
}

void OpenGLWindow::renderLater()
{
	requestUpdate();
}

bool OpenGLWindow::event(QEvent* event)
{
	switch (event->type()) {
	case QEvent::UpdateRequest:
		renderNow();
		return true;
	default:
		return QWindow::event(event);
	}
}

void OpenGLWindow::exposeEvent(QExposeEvent* event)
{
	Q_UNUSED(event);

	if (isExposed())
		renderNow();
}

void OpenGLWindow::renderNow()
{
	if (!isExposed())
		return;

	bool needsInitialize = false;

	if (!m_context) {

		if (nativeCtx.isNull()) {
			qDebug() << "NOT READY FOR RENDER !";
			return;
		}

		m_context = new QOpenGLContext(this);		// Letting gles.cpp initialize context for now bc of annoyances //
		m_context->setNativeHandle(nativeCtx);
		m_context->create();

		needsInitialize = true;
	}

	m_context->makeCurrent(this);

	if (needsInitialize) {
	//	initializeOpenGLFunctions();
		initialize();
	}

	render();

	m_context->swapBuffers(this);

//	if (m_animating)
//		renderLater();
}




void OpenGLWindow::keyReleaseEvent(QKeyEvent* event)
{
	if (Qt::Key_Escape == event->key())
		qApp->quit();
}



#endif





