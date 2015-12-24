#ifndef HYDROCOUPLECOMPOSER_H
#define HYDROCOUPLECOMPOSER_H

#include <QtWidgets/QMainWindow>
#include "ui_hydrocouplecomposer.h"

class HydroCoupleComposer : public QMainWindow
{
	Q_OBJECT

public:
	HydroCoupleComposer(QWidget *parent = 0);
	~HydroCoupleComposer();

private:
	Ui::HydroCoupleComposerClass ui;
};

#endif // HYDROCOUPLECOMPOSER_H
