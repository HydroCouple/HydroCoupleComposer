#include "stdafx.h"
#include "splashscreen.h"

#include <QPainter>

SplashScreen::SplashScreen(const QPixmap & pixmap, Qt::WindowFlags f)
   : QSplashScreen(pixmap, f)
{
   m_color = QColor(0,150,255);
   m_alignment = Qt::AlignCenter | Qt::AlignBottom  ;
   //setMinimumSize(QSize(600,600));
   //connect(this, SIGNAL(messageChanged(const QString&)), this, SLOT(onShowMessage(const QString&)));
}

SplashScreen::SplashScreen(QWidget * parent, const QPixmap & pixmap, Qt::WindowFlags f)
   : QSplashScreen(parent, pixmap, f)
{
   m_color = QColor(0, 150, 255);
   m_alignment = Qt::AlignCenter | Qt::AlignBottom;
}

SplashScreen::~SplashScreen()
{
}

void SplashScreen::setColor(const QColor& color)
{
   m_color = color;
}

void SplashScreen::setAlignment(Qt::Alignment alignment)
{
   m_alignment = alignment;
}


void SplashScreen::drawContents(QPainter * painter)
{
   painter->setPen(m_color);
   painter->setFont(font());
   QRect rect = this->rect();
   rect.adjust(25, 25, -25, -25);
   painter->drawText(rect, m_alignment | Qt::TextWordWrap | Qt::TextJustificationForced, m_message);
   QSplashScreen::drawContents(painter);
}



void SplashScreen::onShowMessage(const QString& message)
{
   m_message = message;
   QSplashScreen::showMessage(QString(""), m_alignment | Qt::TextWordWrap | Qt::TextJustificationForced, m_color);
}
