#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include "ui_connectiondialog.h"

class ConnectionDialog : public QDialog, public Ui::ConnectionDialog
{
	Q_OBJECT

public:
	ConnectionDialog(QDialog *parent = 0);

	virtual ~ConnectionDialog();
};

#endif // CONNECTIONDIALOG_H
