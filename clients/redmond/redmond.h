/*
 * $Id$
 *
 * Redmond KWin client
 *
 * Copyright 2001-2003
 *   Ported to kwin_iii by Chris Lee <clee@kde.org>
 *   Karol Szwed <gallium@kde.org>
 *   http://gallium.n3.net/
 *
 * Based on the default KWin client.
 *
 * Updated to support the new API 9/2003 (CL)
 * Updated to emulate More Accurately 9/2003 (CL)
 * Updated to support toolwindows 3/2001 (KS)
 *
 */

#ifndef __KDE_REDMOND_H
#define __KDE_REDMOND_H

#include <qvariant.h>
#include <qbutton.h>
#include <qbitmap.h>
#include <kpixmap.h>
#include <kdecoration.h>
#include <kdecorationfactory.h>

class QLabel;
class QSpacerItem;
class QBoxLayout;

namespace Redmond {

class RedmondDeco;

class RedmondButton : public QButton
{
	Q_OBJECT
public:
   	RedmondButton(RedmondDeco *parent=0, const char *name=0, 
	              const unsigned char *bitmap=NULL,
	              bool menuButton=false, bool isMini=false,
	              const QString& tip=NULL);
	void setBitmap(const unsigned char *bitmap);
	void setPixmap(const QPixmap &p);
	void reset();

	QSize sizeHint() const;
	int   last_button;

protected:
	void mousePressEvent(QMouseEvent* e);
	void mouseReleaseEvent(QMouseEvent* e);
	virtual void drawButton(QPainter *p);
	void drawButtonLabel(QPainter *){;}

	QBitmap  deco;
	QPixmap  pix;
	bool     menuBtn;
	bool     miniBtn;
	RedmondDeco *client;
};


class RedmondDeco : public KDecoration
{
	Q_OBJECT

public:
	RedmondDeco(KDecorationBridge *, KDecorationFactory *);
	~RedmondDeco() {;}
	void init();

protected:
	void resizeEvent(QResizeEvent*);
	void paintEvent(QPaintEvent*);
	void showEvent(QShowEvent*);
	void mouseDoubleClickEvent(QMouseEvent *);
	void captionChange(const QString& name);
	void maximizeChange(bool m);
	void activeChange(bool);
	void iconChange();
	void calcHiddenButtons();

//	New stuff.
	MousePosition mousePosition(const QPoint &) const;
	void borders(int &, int &, int &, int &) const;
	void resize(const QSize &);
	QSize minimumSize() const;
	void activeChange();
	void captionChange();
	void maximizeChange();
	void desktopChange();
	void shadeChange();
	bool eventFilter(QObject *, QEvent *);

protected slots:
	void slotReset();
	void slotMaximize();
	void menuButtonPressed();

private:
	enum Buttons{ BtnHelp=0, BtnMax, BtnMin, BtnClose, BtnMenu, BtnCount };

	RedmondButton* button[RedmondDeco::BtnCount];
	int            lastButtonWidth;
	int            titleHeight;
	QSpacerItem*   titlebar;
	bool           hiddenItems;
	QBoxLayout*    hb;
	bool           smallButtons;
};

class RedmondDecoFactory : public QObject, public KDecorationFactory
{
   Q_OBJECT
public:
	RedmondDecoFactory();
	virtual ~RedmondDecoFactory();
	virtual KDecoration *createDecoration(KDecorationBridge *);
	virtual bool reset(unsigned long);
};

}

#endif
// vim: ts=4
