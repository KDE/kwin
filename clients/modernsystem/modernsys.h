/*
  Copyright (C) 1999 Daniel M. Duley <mosfet@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
 */

#ifndef __MODERNSYS_H
#define __MODERNSYS_H

#include <QBitmap>
#include <kcommondecoration.h>
#include <kdecorationfactory.h>

namespace ModernSystem {

class ModernSys;

class ModernButton : public KCommonDecorationButton
{
public:
    ModernButton(ButtonType type, ModernSys *parent, const char *name);
    void setBitmap(const unsigned char *bitmap);
    virtual void reset(unsigned long changed);
protected:
    void paintEvent(QPaintEvent *);
    virtual void drawButton(QPainter *p);
    void drawButtonLabel(QPainter *){;}
    QBitmap deco;
};

class ModernSys : public KCommonDecoration
{
public:
    ModernSys( KDecorationBridge* b, KDecorationFactory* f );
    ~ModernSys(){;}

    virtual QString visibleName() const;
    virtual QString defaultButtonsLeft() const;
    virtual QString defaultButtonsRight() const;
    virtual bool decorationBehaviour(DecorationBehaviour behaviour) const;
    virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton * = 0) const;
    virtual KCommonDecorationButton *createButton(ButtonType type);

    virtual void updateWindowShape();
    virtual void updateCaption();

    void init();
protected:
    void drawRoundFrame(QPainter &p, int x, int y, int w, int h);
    void paintEvent( QPaintEvent* );
    void recalcTitleBuffer();
    void reset( unsigned long );
private:
    QPixmap titleBuffer;
    QString oldTitle;
    bool reverse;
};

class ModernSysFactory : public QObject, public KDecorationFactory
{
    Q_OBJECT

public:
    ModernSysFactory();
    virtual ~ModernSysFactory();
    virtual KDecoration* createDecoration( KDecorationBridge* );
    virtual bool reset( unsigned long changed );
    virtual bool supports( Ability ability ) const;
    QList< BorderSize > borderSizes() const;
private:
    void read_config();
};

}

#endif // __MODERNSYS_H
