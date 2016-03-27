#include "stdafx.h"
#include "include/hydrocouplecomposer.h"
#include <QtWidgets/QApplication>
#include "include/splashscreen.h"

void setApplicationStyle(QApplication& application)
{

   qDebug() << QStyleFactory::keys();
   application.setStyle("Fusion");
   QFile file(":/HydroCoupleComposer/Styles");
   file.open(QFile::ReadOnly);
   QString styleSheet = QLatin1String(file.readAll());
   application.setStyleSheet(styleSheet);
   //application.setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
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

      HydroCoupleComposer w;

      //splash screen
      SplashScreen splash(QPixmap(":/HydroCoupleComposer/hydrocouplecomposersplash"));
      splash.setFont(QFont("Segoe UI Light", 10, 2));
      splash.show();


      //!Read settings
      splash.onShowMessage("Reading Settings");

      //!Load components
      splash.onShowMessage("Loading Component Libraries");

      //!Setup component manager
      ComponentManager* manager = w.componentManager();
      manager->connect(manager, SIGNAL(statusChangedMessage(const QString&)), &splash, SLOT(onShowMessage(const QString &)));

#ifdef _WIN32 // note the underscore: without it, it's not msdn official!
      // Windows (x64 and x86)
      manager->addComponentDirectory(QDir("./"));
      manager->addComponentDirectory(QDir("./Components"));

#elif __unix__ // all unices, not all compilers
      // Unix
      manager->addComponentDirectory(QDir("./"));
      manager->addComponentDirectory(QDir("./Components"));
#elif __linux__
      manager->addComponentDirectory(QDir("./"));
      manager->addComponentDirectory(QDir("./Components"));
      // linux
#elif __APPLE__
      manager->addComponentDirectory(QDir("./"));
      manager->addComponentDirectory(QDir("./Components"));
      manager->addComponentDirectory(QDir("./../../../"));
      manager->addComponentDirectory(QDir("./../../../Components"));
#endif

#ifdef QT_DEBUG
     manager->addComponentDirectory(QDir("./../../../../../../HydroCoupleSDK/bin/"));
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
