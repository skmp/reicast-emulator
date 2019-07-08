#include <QtCore>
#include <QDebug>
#include "stdclass.h"
#include "version.h"

#include "Application.h"
#include "MainWindow.h"
#include "VulkanWindow.h"

#include "Renderer_if.h"

void os_DoEvents();

int main(int argc, char* argv[])
{
	QApplication::setAttribute(Qt::AA_NativeWindows);
	QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

	Application app(argc, argv);
	app.setQuitOnLastWindowClosed(true);
	app.setApplicationName("reicast");
	app.setApplicationVersion(REICAST_VERSION);

#if 1
	if (!app.createViewWindow()) {
		qDebug() << "Failed to create main window!";
		return -1;
	}
#endif
#if 0//def REICAST_IN_THREAD		--- will use thread for dc_run() and if want imgui , add draw_single_frame() to timer for 1/30sec
	app.exec();
#else
	
	os_DoEvents();

	//// *FIXME* setup proper osd layer and osd_init, vmem/reservebottomfeeder/exception fuckery for all platforms somethere the fuck else ////
//#ifdef _WIN64
//	setup_seh();
//#endif

	int reicast_init(int argc, char* argv[]);
	void* rend_thread(void*);
	void* dc_run(void*);
	void dc_term();
	int dc_start_game(const char* path);

	set_user_data_dir( QString(QDir::currentPath()).toUtf8().cbegin() );	//  + "/data"  stupid data dir replication bs
	set_user_config_dir("./");

	os_DoEvents();

	if (reicast_init(argc, argv) != 0)
		die("Reicast initialization failed");

	printf("------- finished init-------\n");
	//settings.audio.backend = "XAudio2";

	

#if 0
	//dc_start_game(NULL);
	rend_thread(NULL);
#else

	rend_init_renderer(app.m_renderer);

	os_DoEvents();

	dc_start_game("G:\\Console_Games\\SEGA Dreamcast\\mvc2.gdi");

	rend_term_renderer();
#endif

	dc_term();

#endif
	printf("------- finished -------\n");

	return 0;
}


void os_DoEvents()
{
	for (u32 i = 0; i < 100; i++) {
		QCoreApplication::processEvents();
		QCoreApplication::sendPostedEvents();
	}
	//reiApp->ex
}






// Gamepads
u16 kcode[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

void UpdateInputState(unsigned int uis)
{

}

void* libPvr_GetRenderTarget()
{
	if (reiApp->isGL())
		return reinterpret_cast<void*>(reiApp->rendWindow()->winId());

	return reinterpret_cast<void*>(reiApp->mainWindow()->winId());
}

void* libPvr_GetRenderSurface()
{
	return GetDC((HWND)libPvr_GetRenderTarget());
}



double os_GetSeconds()
{
	static QDateTime saved = QDateTime::currentDateTime();
	return (double)saved.msecsTo(QDateTime::currentDateTime()) / 1000.f;

//	static u64 start = QDateTime::currentSecsSinceEpoch();
//	return QDateTime::currentSecsSinceEpoch() - start;
}

void os_CreateWindow(void) {}
void os_SetupInput(void) {}
void os_DebugBreak(void) {}

int get_mic_data(u8* buffer) { return 0; }
void push_vmu_screen(int bus_id, int bus_port, u8* buffer) { }
