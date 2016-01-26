#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include <QtWidgets/QApplication>
#include "splashscreen.h"

void setApplicationStyle(QApplication& application)
{
	application.setStyle("fusion");
	QFile file(":/HydroCoupleComposer/Styles");
	file.open(QFile::ReadOnly);
	QString styleSheet = QLatin1String(file.readAll());
	application.setStyleSheet(styleSheet);
}


int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		QApplication a(argc, argv);
		a.setOrganizationName("Insuyo");
		a.setOrganizationDomain("www.insuyo.com");
		a.setApplicationName("HydroCoupleComposer");

		//set style
		setApplicationStyle(a);

		//splash screen
		SplashScreen splash(QPixmap(":/HydroCoupleComposer/hydrocouplecomposersplash"));
		splash.setFont(QFont("Segoe UI Light", 10, 2));
		splash.show();

		HydroCoupleComposer w;

		//!Read settings
		splash.onShowMessage("Reading Settings");

		//!Load components
		splash.onShowMessage("Loading Component Libraries");

		//!Setup component manager
		ComponentManager* manager = w.componentManager();
		manager->connect(manager, SIGNAL(statusChangedMessage(const QString&)), &splash, SLOT(onShowMessage(const QString &)));
		QDir dir(a.applicationDirPath());
		manager->addComponentDirectory(dir);

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
		// Windows (x64 and x86)
#elif __unix__ // all unices, not all compilers
		// Unix
#elif __linux__
		// linux
#elif __APPLE__
		if (dir.cdUp() && dir.cdUp() && dir.cdUp())
			manager->addComponentDirectory(dir);
#endif
		splash.finish(&w);
		w.show();
		return a.exec();
	}
	else
	{
		//run project in console finish later
		for (int i = 1; i < argc; i++)
		{
			if (QString(argv[i]) == "-help")
			{

			}
			else if (QString(argv[i]) == "-f")
			{

			}
			else if (QString(argv[i]) == "-v")
			{

			}
			else if (QString(argv[i]) == "-l")
			{

			}
		}
	}

	return 0;
}
