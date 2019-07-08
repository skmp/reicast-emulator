/*
*	qt/OpenGLWindow
*/
#pragma once

#if 0
#include <QApplication>
#include <QKeyEvent>
#include <QtGui/QOpenGLWindow>

class OpenGLWindow
	: public QOpenGLWindow
{
	Q_OBJECT

public:
	OpenGLWindow()
		: QOpenGLWindow()
	{	}

protected:
	void paintGL()
	{
		printf("--------------------------- %s() \n",__FUNCTION__);
		/*

		glClearColor(1.f, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		*/

	}


	//	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event)
	{
		if (Qt::Key_Escape == event->key())
			qApp->quit();
	}


};




#else

/*
*	qt/OpenGLWindow
*/
#pragma once

#include <QWindow>
#include <QKeyEvent>
#include <QtGui/QOpenGLContext>
//#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QPainter>

#ifdef _WIN32 
#include <QtPlatformHeaders/QWGLNativeContext>
#endif



class OpenGLWindow
	: public QWindow
//	, protected QOpenGLFunctions
{
	Q_OBJECT
		
//	bool m_animating;

	QOpenGLContext* m_context = nullptr;
	QOpenGLPaintDevice* m_device = nullptr;



public:

	QVariant nativeCtx;

	static OpenGLWindow* Get();



	explicit OpenGLWindow(QWindow* parent = 0);
	~OpenGLWindow();

	virtual void render(QPainter* painter);
	virtual void render();

	virtual void initialize();

	void setAnimating(bool animating);

public slots:
	void renderLater();
	void renderNow();

protected:
	bool event(QEvent* event) override;

	void exposeEvent(QExposeEvent* event) override;

//	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

};





#endif





