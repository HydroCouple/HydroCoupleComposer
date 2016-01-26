#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QSplashScreen>

class SplashScreen : public QSplashScreen
{
	Q_OBJECT

public:
	SplashScreen(const QPixmap & pixmap = QPixmap(), Qt::WindowFlags f = 0);
	SplashScreen(QWidget * parent, const QPixmap & pixmap = QPixmap(), Qt::WindowFlags f = 0);
	~SplashScreen();

	void setColor(const QColor& color);
	void setAlignment(Qt::Alignment alignment);
	void drawContents(QPainter * painter) override; 

public slots:
	void onShowMessage(const QString& message);


private:
	QColor m_color;
	Qt::Alignment m_alignment;
	QString m_message;
};

#endif // SPLASHSCREEN_H
