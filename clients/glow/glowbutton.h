/***************************************************************************
                          glowbutton.h  -  description
                             -------------------
    begin                : Thu Sep 14 2001
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

#ifndef GLOW_BUTTON_H
#define GLOW_BUTTON_H

#include <vector>
#include <kwin/kwinbutton.h>

class QPixmap;
class QBitmap;
class QTimer;

namespace KWinInternal
{

class GlowButton : public KWinWidgetButton
{
	Q_OBJECT

public:
	GlowButton(QWidget *parent, const char* name,
		const QString& tip, QPixmap *bgPixmap=0);
	~GlowButton();

	QPixmap *getPixmap();

	/**
	 * Sets the background pixmap of the button.
	 * The pixmap should consist of sub pixmaps of the size of the button which
	 * are placed one below the other. Each sub pixmap is copied on the button
	 * in succession to create the glow effect. The last sub pixmap is used
	 * when the button is pressed.
	 */
	void setPixmap(QPixmap* pixmap);

protected:
	virtual void paintEvent( QPaintEvent * );
	virtual void enterEvent( QEvent * );
	virtual void leaveEvent( QEvent * );
	virtual void mousePressEvent( QMouseEvent * );
	virtual void mouseReleaseEvent( QMouseEvent * );

protected slots:
	void slotTimeout();

private:
	enum TimerStatus { Run, Stop };

	int m_updateTime;
	int m_steps;

	QPixmap *m_pixmap;
	QTimer *m_timer;
	int m_pos;
	TimerStatus m_timerStatus;

signals:
	void clicked();
	void clicked(int button);
};

//-----------------------------------------------------------------------------

class GlowButtonFactory
{
public:
	GlowButtonFactory();

	int getSteps();

	/**
	 * Sets the number of pixmaps used to create the glow effect of the
	 * glow buttons.
	 */
	void setSteps(int steps);

	QPixmap* createGlowButtonPixmap(
		const QSize& size, const QColor& glowColor,
		const QColorGroup& colorGroup, const QPixmap& fgPixmap );
	QPixmap* createGlowButtonPixmap(
		const QSize& size, const QPixmap& glowPixmap,
		const QColorGroup& colorGroup, const QPixmap& fgPixmap );
	GlowButton* createGlowButton(
		QWidget *parent, const char* name,
		const QString& tip, QPixmap *pixmap=0);

private:
	int m_steps;
};

//-----------------------------------------------------------------------------

class DrawUtils
{
public:
	static QPixmap fadePixmaps(
		const QPixmap& bgPixmap, const QPixmap& glowPixmap,
		const vector< vector<float> >& intensityField, float intensity );
	static QPixmap drawSimpleRoundButton(
		const QSize& size, const QColorGroup& group);
	static QBitmap drawSimpleRoundButtonMask(const QSize& size);
};

}; // namespace

#endif
