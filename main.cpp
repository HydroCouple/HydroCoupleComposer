#include "stdafx.h"
#include "hydrocouplecomposer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	HydroCoupleComposer w;
	w.show();
	return a.exec();
}
