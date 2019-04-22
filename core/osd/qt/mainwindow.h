/*
**	reicast: ./core/osd/qt/mainwindow.h
*/
#pragma once

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <QtOpenGL>

//#include <QMainWindow>

class MainWindow
	: QWindow
{
	Q_OBJECT

	QThread* emuThread;
	QWindow* renderWindow;	// Should find 'proper' way to switch to QWidget, it's a pain with GL/Vk 

public:
	~MainWindow();
	MainWindow();

	static MainWindow* Get() {
		return qobject_cast<MainWindow*>(qApp->topLevelWindows().at(0));
	}

protected:


	void keyReleaseEvent(QKeyEvent *event) {
		if (Qt::Key_Escape == event->key())
			qApp->exit();
	}


};

