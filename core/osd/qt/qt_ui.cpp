/*
 *	$OSD$/qt/qt_ui.cpp
 *
*/
#include "stdclass.h"
#include "version.h"
#include "types.h"
#include "oslib.h"
#include "cfg/cfg.h"
#include "mem/_vmem.h"
#include "sh4/sh4_mem.h"
#include "audiobackend/audiostream.h"

#include "qt_ui.h"

#if !defined(USE_QT)
#error Building __FILE__ and USE_QT is not set!
#endif


#include <QtGui/QVulkanWindow>

void _init_dc();


size_t operator""_mb(unsigned long long megabytes) { return megabytes * 1024 * 1024; }

size_t nn = 5_mb;

// We'll stick out main loop in here too //

int main(int argc, char *argv[])
{
	QApplication app(argc,argv);
	app.setApplicationName("reicast");
	app.setApplicationVersion(REICAST_VERSION);
	app.setQuitOnLastWindowClosed(true);

	// Should prob parse cmd line //
	//_init_dc();
	qDebug() << "WTF nn: " << nn;


	MainWindow *mainWindow = new MainWindow;
	return app.exec();
}



MainWindow::~MainWindow()
{
	renderWindow->deleteLater();
}

MainWindow::MainWindow()
	: QWindow()
	, renderWindow(NULL)
{
	int useGL = 1;

	if (useGL) {
		QSurfaceFormat format;
		format.setDepthBufferSize(24);
		format.setStencilBufferSize(8);
		format.setVersion(3, 2);
		format.setProfile(QSurfaceFormat::CoreProfile);
		QSurfaceFormat::setDefaultFormat(format);

		
		renderWindow = qobject_cast<QWindow*>(new QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, this));
	}
	else renderWindow = qobject_cast<QWindow*>(new QVulkanWindow(this));


	renderWindow->show();
	show();

}










void _init_dc()
{

	if (!_vmem_reserve())
		qFatal("failed to alloc mem");


	if (!cfgOpen())
		qWarning("failed to open cfg\n");

	LoadSettings();
	if (!LoadRomFiles(get_readonly_data_path("./data/")))
		qFatal("Failed to load dc bios or flash files");

	if (rv_ok != plugins_Init())
		qFatal("plugin init failed");


#if FEAT_SHREC != DYNAREC_NONE
	if (settings.dynarec.Enable) {
		Get_Sh4Recompiler(&sh4_cpu);
		qInfo("Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		qInfo("Using Interpreter\n");
	}

	InitAudio();

	sh4_cpu.Init();
	mem_Init();
	mem_map_default();

#if DC_PLATFORM == DC_PLATFORM_NAOMI
	mcfg_CreateNAOMIJamma();
#endif

	plugins_Reset(false);
	mem_Reset(false);

	//sh4_cpu.Reset(false);

}




int __cdecl msgboxf(const char* text, unsigned int type, ...)
{
	va_list args;

	char temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);


	QMessageBox::information(0, "", temp, QMessageBox::Ok);
	return 0;
}



// This is a very shady looking function, bound to crash if someone uses it w.o realizing you need to pass in a buffer
// ;shrug; *FIXME* why aren't we using std::string at least?
int GetFile(char *szFileName, char *szParse = 0, u32 flags = 0)
{
	cfgLoadStr("config", "image", szFileName, "null");
	if (strcmp(szFileName, "null") == 0)
	{
		QString filePath = QFileDialog::getOpenFileName(nullptr, "GetFile()", QDir::currentPath());
		if (!filePath.isEmpty())
			strcpy(szFileName, filePath.toUtf8().cbegin());
	}
	return 1;
}


// bool getFile(std::string& filePath) { }



