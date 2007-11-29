/********************************************************************
 
  Redmond KWin client
 
  Copyright 2001-2003
    Ported to kwin_iii by Chris Lee <clee@kde.org>
    Karol Szwed <gallium@kde.org>
    http://gallium.n3.net/
 
  Based on the default KWin client.
 
  Updated to support the new API 9/2003 (CL)
  Updated to emulate More Accurately 9/2003 (CL)
  Updated to support toolwindows 3/2001 (KS)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __KDE_REDMOND_H
#define __KDE_REDMOND_H

#include <QBitmap>
#include <kcommondecoration.h>
#include <kdecorationfactory.h>

namespace Redmond {

class RedmondDeco;

class RedmondButton : public KCommonDecorationButton
{
	Q_OBJECT
public:
	RedmondButton(ButtonType type, RedmondDeco *parent);
	void setBitmap(const unsigned char *bitmap);
	void setPixmap(const QPixmap &p);
	void reset(unsigned long changed);

protected:
	void paintEvent(QPaintEvent *);
	virtual void drawButton(QPainter *p);
	void drawButtonLabel(QPainter *){;}

	QBitmap  deco;
	QPixmap  pix;
	bool     miniBtn;
};


class RedmondDeco : public KCommonDecoration
{
public:
	RedmondDeco(KDecorationBridge *, KDecorationFactory *);
	~RedmondDeco() {;}

	virtual QString visibleName() const;
	virtual QString defaultButtonsLeft() const;
	virtual QString defaultButtonsRight() const;
	virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
	virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
	virtual KCommonDecorationButton *createButton(ButtonType type);

	void init();

protected:
    virtual void reset( unsigned long changed );

	void paintEvent(QPaintEvent*);

private:
	int            titleHeight;
};

class RedmondDecoFactory : public QObject, public KDecorationFactory
{
   Q_OBJECT
public:
	RedmondDecoFactory();
	virtual ~RedmondDecoFactory();
	virtual KDecoration *createDecoration(KDecorationBridge *);
	virtual bool reset(unsigned long);
	virtual bool supports( Ability ability ) const;
	virtual QList< BorderSize > borderSizes() const;
private:
	void readConfig();
};

}

#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
