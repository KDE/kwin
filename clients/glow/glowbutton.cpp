/***************************************************************************
                          glowbutton.cpp  -  description
                             -------------------
    begin                : Thu Sep 6 2001
    copyright            : (C) 2001 by Henning Burchardt
    email                : h_burchardt@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <vector>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qpainter.h>
#include <qimage.h>
#include <qtimer.h>
#include "glowbutton.h"

using namespace KWinInternal;

//-----------------------------------------------------------------------------

GlowButton::GlowButton(QWidget *parent, const char *name,
	const QString& tip, QPixmap *pixmap)
	: KWinWidgetButton(parent, name, 0, tip), m_pixmap(pixmap)
{
	m_steps = 0;
	m_updateTime = 50;

	m_timer = new QTimer();
	QObject::connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
	m_pos = 0;
	m_timerStatus = Stop;
}

GlowButton::~GlowButton()
{
}

QPixmap *GlowButton::getPixmap()
{
	return m_pixmap;
}

void GlowButton::setPixmap(QPixmap *pixmap)
{
	m_pixmap = pixmap;

	// set mask
	QBitmap mask(width(), height());
	mask.fill(Qt::color0);
	bitBlt(&mask, 0, 0, pixmap->mask(), 0, 0, width(), height());
	setMask(mask);

	// set steps
	m_steps = pixmap->height()/pixmap->width() - 1;

	repaint(false);
}

void GlowButton::paintEvent( QPaintEvent *e )
{
 	QWidget::paintEvent(e);
	if( m_pixmap != 0 )
	{
		int pos = m_pos>=0?m_pos:-m_pos;
		bitBlt(this, 0, 0, m_pixmap, 0, pos*height(), width(), height());
	}
}

void GlowButton::enterEvent( QEvent *e )
{
	if( m_pos<0 )
		m_pos=-m_pos;
	m_timerStatus = Run;
	if( ! m_timer->isActive() )
		m_timer->start(m_updateTime);
	KWinWidgetButton::enterEvent(e);
}

void GlowButton::leaveEvent( QEvent *e )
{
	m_timerStatus = Stop;
	if( ! m_timer->isActive() )
		m_timer->start(m_updateTime);
	KWinWidgetButton::leaveEvent(e);
}

void GlowButton::mousePressEvent( QMouseEvent *e )
{
	if( m_timer->isActive() )
		m_timer->stop();
	m_pos = m_steps;
	repaint(false);
//	emit pressed();
	KWinWidgetButton::mousePressEvent(e);
}

void GlowButton::mouseReleaseEvent( QMouseEvent *e )
{
	QPoint p = mapToParent(mapFromGlobal(e->globalPos()));
	if( ! geometry().contains(p) )
		m_timerStatus = Stop;
	else
	{
		emit clicked();
		emit clicked(e->button());
	}
	if( ! m_timer->isActive() )
		m_timer->start(m_updateTime);
//	emit released();
	KWinWidgetButton::mouseReleaseEvent(e);
}

void GlowButton::slotTimeout()
{
	repaint(false);

	if( m_pos>=m_steps-1 )
		m_pos = -m_pos;

	if( m_timerStatus==Stop )
	{
		if( m_pos==0 )
		{
			m_timer->stop();
			return;
		}
		else if( m_pos>0 )
			m_pos = -m_pos;
	}

	m_pos++;
}

//-----------------------------------------------------------------------------

GlowButtonFactory::GlowButtonFactory()
{
	m_steps = 20;
}

int GlowButtonFactory::getSteps()
{
	return m_steps;
}

void GlowButtonFactory::setSteps(int steps)
{
	m_steps = steps;
}

QPixmap* GlowButtonFactory::createGlowButtonPixmap(
	const QSize& size, const QColor& glowColor,
	const QColorGroup& colorGroup, const QPixmap& fgPixmap )
{
	QPixmap pm(size);
	pm.fill(glowColor);
	return createGlowButtonPixmap(
		size, pm, colorGroup, fgPixmap);
}

QPixmap* GlowButtonFactory::createGlowButtonPixmap(
	const QSize& size, const QPixmap& glowPixmap,
	const QColorGroup& colorGroup, const QPixmap& fgPixmap)
{
// cerr << "GlowButtonFactory " << "createGlowButtonPixmap " << endl;
	int w = size.width();
	int h = size.height();

	vector< vector<float> > intensityField;
	intensityField.resize(h);
	for( int i=0; i<h; i++ )
	{
		intensityField[i].resize(w);
		for( int j=0; j<w; j++ )
		{
			float distance = 0;
//			float distance = sqrt(
//				((w/2.0-j)*(w/2.0-j)+(h/2.0-i)*(h/2.0-i))/(w*w/4.0+h*h/4.0) );
			intensityField[i][j] = (1-distance);
		}
	}

	QPainter painter;
	QPixmap upPixmap(w, h);
	upPixmap = DrawUtils::drawSimpleRoundButton(size, colorGroup);
//	upPixmap.fill(colorGroup.background());
	painter.begin(&upPixmap);
//	QApplication::style().drawButton(
//		&painter, 0, 0, w, h, colorGroup, false );
	painter.drawPixmap(0, 0, fgPixmap);
	painter.end();

	QPixmap downPixmap(w, h);
	downPixmap = DrawUtils::drawSimpleRoundButton(size, colorGroup);
//	downPixmap.fill(colorGroup.background());
	painter.begin(&downPixmap);
//	QApplication::style().drawButton(
//		&painter, 0, 0, w, h, colorGroup, true );
	painter.drawPixmap(1, 1, fgPixmap);
	painter.end();

	QPixmap *pm = new QPixmap(w, (m_steps+1)*h);
	QPixmap fadedPixmap;
	for( int i=0; i<m_steps; i++ )
	{
//		float intensity = (float) i/m_steps;
		float intensity = (float) i/m_steps/2;
		fadedPixmap = DrawUtils::fadePixmaps(upPixmap, glowPixmap,
			intensityField, intensity);
		bitBlt(pm, 0, i*h, &fadedPixmap);
	}
	fadedPixmap = DrawUtils::fadePixmaps(downPixmap, glowPixmap,
		intensityField, 0.5f);
	bitBlt(pm, 0, m_steps*h, &fadedPixmap);

	QBitmap simpleMask = DrawUtils::drawSimpleRoundButtonMask(size);
	QBitmap mask(w, (m_steps+1)*h);
	mask.fill(Qt::color0);
//	painter.begin(&mask);
	for( int i=0; i<m_steps+1; i++ )
		bitBlt(&mask, 0, i*h, &simpleMask);
//		QApplication::style().drawButtonMask( &painter, 0, i*h, w, h );
//	painter.end();

	pm->setMask(mask);
	return pm;
}

GlowButton* GlowButtonFactory::createGlowButton(
	QWidget *parent, const char* name, const QString& tip, QPixmap *pixmap)
{
	GlowButton *glowButton = new GlowButton(
		parent, name, tip, pixmap);
	return glowButton;
}

//-----------------------------------------------------------------------------

QPixmap DrawUtils::fadePixmaps(
	const QPixmap& bgPixmap, const QPixmap& fgPixmap,
	const vector< vector<float> >& intensityField, float intensity )
{
// cerr << "DrawUtils " << "fadePixmaps " << endl;
	QImage bgImg = bgPixmap.convertToImage();
	QImage fgImg = fgPixmap.convertToImage();
	QImage newImg(bgImg.width(), bgImg.height(), bgImg.depth());
	int w = bgImg.width();
	int h = bgImg.height();
	for( int y=0; y<h; y++ )
	{
		uint* bgLine = (uint*) bgImg.scanLine(y);
		uint* fgLine = (uint*) fgImg.scanLine(y);
		uint* newLine = (uint*) newImg.scanLine(y);
		for( int x=0; x<w; x++ )
		{
			QRgb bgRgb = *(bgLine+x);
			uint bgRed = qRed(bgRgb);
			uint bgGreen = qGreen(bgRgb);
			uint bgBlue = qBlue(bgRgb);
			QRgb fgRgb = *(fgLine+x);
			uint fgRed = qRed(fgRgb);
			uint fgGreen = qGreen(fgRgb);
			uint fgBlue = qBlue(fgRgb);
			float factor = intensityField[y][x]*intensity;
			uint newRed = (uint) ((1-factor)*bgRed + factor*fgRed);
			uint newGreen = (uint) ((1-factor)*bgGreen + factor*fgGreen);
			uint newBlue = (uint) ((1-factor)*bgBlue + factor*fgBlue);
			*(newLine+x) = qRgb( newRed, newGreen, newBlue );
		}
	}

	QPixmap newPixmap;
	newPixmap.convertFromImage(newImg);
	return newPixmap;
}

QPixmap DrawUtils::drawSimpleRoundButton(
	const QSize& size, const QColorGroup& colorGroup)
{
	QColor mid = colorGroup.button();
	QColor border = colorGroup.light();
	int w = size.width();
	int h = size.height();

	QPainter p;

	vector< vector<float> > intensityField;
	intensityField.resize(h);
	for( int i=0; i<h; i++ )
	{
		intensityField[i].resize(w);
		for( int j=0; j<w; j++ )
		{
			float distance = sqrt(
				((w/2.0-j)*(w/2.0-j)+(h/2.0-i)*(h/2.0-i))/(w*w/4.0+h*h/4.0) );
			intensityField[i][j] = (1-distance);
		}
	}

	QPixmap darkPm(size);
	darkPm.fill(mid);
	QPixmap lightPm(size);
	lightPm.fill(border);

	QPixmap pm = fadePixmaps(lightPm, darkPm, intensityField, 1.0);

	p.begin(&pm);
	int hh, ss, vv;
	mid.hsv(&hh, &ss, &vv);
	mid.setHsv(hh, 0, vv);
	p.setPen(colorGroup.mid());
	p.drawEllipse(0, 0, w, h);
	p.end();

	pm.setMask(drawSimpleRoundButtonMask(size));

	return pm;
}

QBitmap DrawUtils::drawSimpleRoundButtonMask( const QSize& size )
{
	int w = size.width();
	int h = size.height();
	QPainter p;
	QBitmap mask(size);
	mask.fill(Qt::color0);
	p.begin(&mask);
	p.setPen(Qt::color1);
	p.setBrush(Qt::color1);
	p.drawEllipse(0, 0, w, h);
	p.end();
	return mask;
}

#include "glowbutton.moc"

